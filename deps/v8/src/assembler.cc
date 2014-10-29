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

#include <cmath>
#include "src/api.h"
#include "src/base/cpu.h"
#include "src/base/lazy-instance.h"
#include "src/base/platform/platform.h"
#include "src/builtins.h"
#include "src/codegen.h"
#include "src/counters.h"
#include "src/cpu-profiler.h"
#include "src/debug.h"
#include "src/deoptimizer.h"
#include "src/execution.h"
#include "src/ic/ic.h"
#include "src/ic/stub-cache.h"
#include "src/isolate-inl.h"
#include "src/jsregexp.h"
#include "src/regexp-macro-assembler.h"
#include "src/regexp-stack.h"
#include "src/runtime/runtime.h"
#include "src/serialize.h"
#include "src/token.h"

#if V8_TARGET_ARCH_IA32
#include "src/ia32/assembler-ia32-inl.h"  // NOLINT
#elif V8_TARGET_ARCH_X64
#include "src/x64/assembler-x64-inl.h"  // NOLINT
#elif V8_TARGET_ARCH_ARM64
#include "src/arm64/assembler-arm64-inl.h"  // NOLINT
#elif V8_TARGET_ARCH_ARM
#include "src/arm/assembler-arm-inl.h"  // NOLINT
#elif V8_TARGET_ARCH_MIPS
#include "src/mips/assembler-mips-inl.h"  // NOLINT
#elif V8_TARGET_ARCH_MIPS64
#include "src/mips64/assembler-mips64-inl.h"  // NOLINT
#elif V8_TARGET_ARCH_X87
#include "src/x87/assembler-x87-inl.h"  // NOLINT
#else
#error "Unknown architecture."
#endif

// Include native regexp-macro-assembler.
#ifndef V8_INTERPRETED_REGEXP
#if V8_TARGET_ARCH_IA32
#include "src/ia32/regexp-macro-assembler-ia32.h"  // NOLINT
#elif V8_TARGET_ARCH_X64
#include "src/x64/regexp-macro-assembler-x64.h"  // NOLINT
#elif V8_TARGET_ARCH_ARM64
#include "src/arm64/regexp-macro-assembler-arm64.h"  // NOLINT
#elif V8_TARGET_ARCH_ARM
#include "src/arm/regexp-macro-assembler-arm.h"  // NOLINT
#elif V8_TARGET_ARCH_MIPS
#include "src/mips/regexp-macro-assembler-mips.h"  // NOLINT
#elif V8_TARGET_ARCH_MIPS64
#include "src/mips64/regexp-macro-assembler-mips64.h"  // NOLINT
#elif V8_TARGET_ARCH_X87
#include "src/x87/regexp-macro-assembler-x87.h"  // NOLINT
#else  // Unknown architecture.
#error "Unknown architecture."
#endif  // Target architecture.
#endif  // V8_INTERPRETED_REGEXP

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// Common double constants.

struct DoubleConstant BASE_EMBEDDED {
double min_int;
double one_half;
double minus_one_half;
double negative_infinity;
double canonical_non_hole_nan;
double the_hole_nan;
double uint32_bias;
};

static DoubleConstant double_constants;

const char* const RelocInfo::kFillerCommentString = "DEOPTIMIZATION PADDING";

static bool math_exp_data_initialized = false;
static base::Mutex* math_exp_data_mutex = NULL;
static double* math_exp_constants_array = NULL;
static double* math_exp_log_table_array = NULL;

// -----------------------------------------------------------------------------
// Implementation of AssemblerBase

AssemblerBase::AssemblerBase(Isolate* isolate, void* buffer, int buffer_size)
    : isolate_(isolate),
      jit_cookie_(0),
      enabled_cpu_features_(0),
      emit_debug_code_(FLAG_debug_code),
      predictable_code_size_(false),
      // We may use the assembler without an isolate.
      serializer_enabled_(isolate && isolate->serializer_enabled()) {
  if (FLAG_mask_constants_with_cookie && isolate != NULL)  {
    jit_cookie_ = isolate->random_number_generator()->NextInt();
  }
  own_buffer_ = buffer == NULL;
  if (buffer_size == 0) buffer_size = kMinimalBufferSize;
  DCHECK(buffer_size > 0);
  if (own_buffer_) buffer = NewArray<byte>(buffer_size);
  buffer_ = static_cast<byte*>(buffer);
  buffer_size_ = buffer_size;

  pc_ = buffer_;
}


AssemblerBase::~AssemblerBase() {
  if (own_buffer_) DeleteArray(buffer_);
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
  // TODO(svenpanne) Remove the 'if' when everything works.
  if (expected_size_ >= 0) {
    CHECK_EQ(expected_size_, assembler_->pc_offset() - start_offset_);
  }
  assembler_->set_predictable_code_size(old_value_);
}


// -----------------------------------------------------------------------------
// Implementation of CpuFeatureScope

#ifdef DEBUG
CpuFeatureScope::CpuFeatureScope(AssemblerBase* assembler, CpuFeature f)
    : assembler_(assembler) {
  DCHECK(CpuFeatures::IsSupported(f));
  old_enabled_ = assembler_->enabled_cpu_features();
  uint64_t mask = static_cast<uint64_t>(1) << f;
  // TODO(svenpanne) This special case below doesn't belong here!
#if V8_TARGET_ARCH_ARM
  // ARMv7 is implied by VFP3.
  if (f == VFP3) {
    mask |= static_cast<uint64_t>(1) << ARMv7;
  }
#endif
  assembler_->set_enabled_cpu_features(old_enabled_ | mask);
}


CpuFeatureScope::~CpuFeatureScope() {
  assembler_->set_enabled_cpu_features(old_enabled_);
}
#endif


bool CpuFeatures::initialized_ = false;
unsigned CpuFeatures::supported_ = 0;
unsigned CpuFeatures::cache_line_size_ = 0;


// -----------------------------------------------------------------------------
// Implementation of Label

int Label::pos() const {
  if (pos_ < 0) return -pos_ - 1;
  if (pos_ > 0) return  pos_ - 1;
  UNREACHABLE();
  return 0;
}


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
//                             [6-bit data delta] [2-bit data type tag]
//
//   11: long_record           [2-bit high tag][4 bit middle_tag] 11
//                             followed by variable data depending on type.
//
//  2-bit data type tags, used in short_data_record and data_jump long_record:
//   code_target_with_id: 00
//   position:            01
//   statement_position:  10
//   comment:             11 (not used in short_data_record)
//
//  Long record format:
//    4-bit middle_tag:
//      0000 - 1100 : Short record for RelocInfo::Mode middle_tag + 2
//         (The middle_tag encodes rmode - RelocInfo::LAST_COMPACT_ENUM,
//          and is between 0000 and 1100)
//        The format is:
//                              00 [4 bit middle_tag] 11 followed by
//                              00 [6 bit pc delta]
//
//      1101: constant or veneer pool. Used only on ARM and ARM64 for now.
//        The format is:       [2-bit sub-type] 1101 11
//                             signed int (size of the pool).
//          The 2-bit sub-types are:
//            00: constant pool
//            01: veneer pool
//      1110: long_data_record
//        The format is:       [2-bit data_type_tag] 1110 11
//                             signed intptr_t, lowest byte written first
//                             (except data_type code_target_with_id, which
//                             is followed by a signed int, not intptr_t.)
//
//      1111: long_pc_jump
//        The format is:
//          pc-jump:             00 1111 11,
//                               00 [6 bits pc delta]
//        or
//          pc-jump (variable length):
//                               01 1111 11,
//                               [7 bits data] 0
//                                  ...
//                               [7 bits data] 1
//               (Bits 6..31 of pc delta, with leading zeroes
//                dropped, and last non-zero chunk tagged with 1.)


#ifdef DEBUG
const int kMaxStandardNonCompactModes = 14;
#endif

const int kTagBits = 2;
const int kTagMask = (1 << kTagBits) - 1;
const int kExtraTagBits = 4;
const int kLocatableTypeTagBits = 2;
const int kSmallDataBits = kBitsPerByte - kLocatableTypeTagBits;

const int kEmbeddedObjectTag = 0;
const int kCodeTargetTag = 1;
const int kLocatableTag = 2;
const int kDefaultTag = 3;

const int kPCJumpExtraTag = (1 << kExtraTagBits) - 1;

const int kSmallPCDeltaBits = kBitsPerByte - kTagBits;
const int kSmallPCDeltaMask = (1 << kSmallPCDeltaBits) - 1;
const int RelocInfo::kMaxSmallPCDelta = kSmallPCDeltaMask;

const int kVariableLengthPCJumpTopTag = 1;
const int kChunkBits = 7;
const int kChunkMask = (1 << kChunkBits) - 1;
const int kLastChunkTagBits = 1;
const int kLastChunkTagMask = 1;
const int kLastChunkTag = 1;


const int kDataJumpExtraTag = kPCJumpExtraTag - 1;

const int kCodeWithIdTag = 0;
const int kNonstatementPositionTag = 1;
const int kStatementPositionTag = 2;
const int kCommentTag = 3;

const int kPoolExtraTag = kPCJumpExtraTag - 2;
const int kConstPoolTag = 0;
const int kVeneerPoolTag = 1;


uint32_t RelocInfoWriter::WriteVariableLengthPCJump(uint32_t pc_delta) {
  // Return if the pc_delta can fit in kSmallPCDeltaBits bits.
  // Otherwise write a variable length PC jump for the bits that do
  // not fit in the kSmallPCDeltaBits bits.
  if (is_uintn(pc_delta, kSmallPCDeltaBits)) return pc_delta;
  WriteExtraTag(kPCJumpExtraTag, kVariableLengthPCJumpTopTag);
  uint32_t pc_jump = pc_delta >> kSmallPCDeltaBits;
  DCHECK(pc_jump > 0);
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


void RelocInfoWriter::WriteTaggedPC(uint32_t pc_delta, int tag) {
  // Write a byte of tagged pc-delta, possibly preceded by var. length pc-jump.
  pc_delta = WriteVariableLengthPCJump(pc_delta);
  *--pos_ = pc_delta << kTagBits | tag;
}


void RelocInfoWriter::WriteTaggedData(intptr_t data_delta, int tag) {
  *--pos_ = static_cast<byte>(data_delta << kLocatableTypeTagBits | tag);
}


void RelocInfoWriter::WriteExtraTag(int extra_tag, int top_tag) {
  *--pos_ = static_cast<int>(top_tag << (kTagBits + kExtraTagBits) |
                             extra_tag << kTagBits |
                             kDefaultTag);
}


void RelocInfoWriter::WriteExtraTaggedPC(uint32_t pc_delta, int extra_tag) {
  // Write two-byte tagged pc-delta, possibly preceded by var. length pc-jump.
  pc_delta = WriteVariableLengthPCJump(pc_delta);
  WriteExtraTag(extra_tag, 0);
  *--pos_ = pc_delta;
}


void RelocInfoWriter::WriteExtraTaggedIntData(int data_delta, int top_tag) {
  WriteExtraTag(kDataJumpExtraTag, top_tag);
  for (int i = 0; i < kIntSize; i++) {
    *--pos_ = static_cast<byte>(data_delta);
    // Signed right shift is arithmetic shift.  Tested in test-utils.cc.
    data_delta = data_delta >> kBitsPerByte;
  }
}


void RelocInfoWriter::WriteExtraTaggedPoolData(int data, int pool_type) {
  WriteExtraTag(kPoolExtraTag, pool_type);
  for (int i = 0; i < kIntSize; i++) {
    *--pos_ = static_cast<byte>(data);
    // Signed right shift is arithmetic shift.  Tested in test-utils.cc.
    data = data >> kBitsPerByte;
  }
}


void RelocInfoWriter::WriteExtraTaggedData(intptr_t data_delta, int top_tag) {
  WriteExtraTag(kDataJumpExtraTag, top_tag);
  for (int i = 0; i < kIntptrSize; i++) {
    *--pos_ = static_cast<byte>(data_delta);
    // Signed right shift is arithmetic shift.  Tested in test-utils.cc.
    data_delta = data_delta >> kBitsPerByte;
  }
}


void RelocInfoWriter::Write(const RelocInfo* rinfo) {
#ifdef DEBUG
  byte* begin_pos = pos_;
#endif
  DCHECK(rinfo->rmode() < RelocInfo::NUMBER_OF_MODES);
  DCHECK(rinfo->pc() - last_pc_ >= 0);
  DCHECK(RelocInfo::LAST_STANDARD_NONCOMPACT_ENUM - RelocInfo::LAST_COMPACT_ENUM
         <= kMaxStandardNonCompactModes);
  // Use unsigned delta-encoding for pc.
  uint32_t pc_delta = static_cast<uint32_t>(rinfo->pc() - last_pc_);
  RelocInfo::Mode rmode = rinfo->rmode();

  // The two most common modes are given small tags, and usually fit in a byte.
  if (rmode == RelocInfo::EMBEDDED_OBJECT) {
    WriteTaggedPC(pc_delta, kEmbeddedObjectTag);
  } else if (rmode == RelocInfo::CODE_TARGET) {
    WriteTaggedPC(pc_delta, kCodeTargetTag);
    DCHECK(begin_pos - pos_ <= RelocInfo::kMaxCallSize);
  } else if (rmode == RelocInfo::CODE_TARGET_WITH_ID) {
    // Use signed delta-encoding for id.
    DCHECK(static_cast<int>(rinfo->data()) == rinfo->data());
    int id_delta = static_cast<int>(rinfo->data()) - last_id_;
    // Check if delta is small enough to fit in a tagged byte.
    if (is_intn(id_delta, kSmallDataBits)) {
      WriteTaggedPC(pc_delta, kLocatableTag);
      WriteTaggedData(id_delta, kCodeWithIdTag);
    } else {
      // Otherwise, use costly encoding.
      WriteExtraTaggedPC(pc_delta, kPCJumpExtraTag);
      WriteExtraTaggedIntData(id_delta, kCodeWithIdTag);
    }
    last_id_ = static_cast<int>(rinfo->data());
  } else if (RelocInfo::IsPosition(rmode)) {
    // Use signed delta-encoding for position.
    DCHECK(static_cast<int>(rinfo->data()) == rinfo->data());
    int pos_delta = static_cast<int>(rinfo->data()) - last_position_;
    int pos_type_tag = (rmode == RelocInfo::POSITION) ? kNonstatementPositionTag
                                                      : kStatementPositionTag;
    // Check if delta is small enough to fit in a tagged byte.
    if (is_intn(pos_delta, kSmallDataBits)) {
      WriteTaggedPC(pc_delta, kLocatableTag);
      WriteTaggedData(pos_delta, pos_type_tag);
    } else {
      // Otherwise, use costly encoding.
      WriteExtraTaggedPC(pc_delta, kPCJumpExtraTag);
      WriteExtraTaggedIntData(pos_delta, pos_type_tag);
    }
    last_position_ = static_cast<int>(rinfo->data());
  } else if (RelocInfo::IsComment(rmode)) {
    // Comments are normally not generated, so we use the costly encoding.
    WriteExtraTaggedPC(pc_delta, kPCJumpExtraTag);
    WriteExtraTaggedData(rinfo->data(), kCommentTag);
    DCHECK(begin_pos - pos_ >= RelocInfo::kMinRelocCommentSize);
  } else if (RelocInfo::IsConstPool(rmode) || RelocInfo::IsVeneerPool(rmode)) {
      WriteExtraTaggedPC(pc_delta, kPCJumpExtraTag);
      WriteExtraTaggedPoolData(static_cast<int>(rinfo->data()),
                               RelocInfo::IsConstPool(rmode) ? kConstPoolTag
                                                             : kVeneerPoolTag);
  } else {
    DCHECK(rmode > RelocInfo::LAST_COMPACT_ENUM);
    int saved_mode = rmode - RelocInfo::LAST_COMPACT_ENUM;
    // For all other modes we simply use the mode as the extra tag.
    // None of these modes need a data component.
    DCHECK(saved_mode < kPCJumpExtraTag && saved_mode < kDataJumpExtraTag);
    WriteExtraTaggedPC(pc_delta, saved_mode);
  }
  last_pc_ = rinfo->pc();
#ifdef DEBUG
  DCHECK(begin_pos - pos_ <= kMaxSize);
#endif
}


inline int RelocIterator::AdvanceGetTag() {
  return *--pos_ & kTagMask;
}


inline int RelocIterator::GetExtraTag() {
  return (*pos_ >> kTagBits) & ((1 << kExtraTagBits) - 1);
}


inline int RelocIterator::GetTopTag() {
  return *pos_ >> (kTagBits + kExtraTagBits);
}


inline void RelocIterator::ReadTaggedPC() {
  rinfo_.pc_ += *pos_ >> kTagBits;
}


inline void RelocIterator::AdvanceReadPC() {
  rinfo_.pc_ += *--pos_;
}


void RelocIterator::AdvanceReadId() {
  int x = 0;
  for (int i = 0; i < kIntSize; i++) {
    x |= static_cast<int>(*--pos_) << i * kBitsPerByte;
  }
  last_id_ += x;
  rinfo_.data_ = last_id_;
}


void RelocIterator::AdvanceReadPoolData() {
  int x = 0;
  for (int i = 0; i < kIntSize; i++) {
    x |= static_cast<int>(*--pos_) << i * kBitsPerByte;
  }
  rinfo_.data_ = x;
}


void RelocIterator::AdvanceReadPosition() {
  int x = 0;
  for (int i = 0; i < kIntSize; i++) {
    x |= static_cast<int>(*--pos_) << i * kBitsPerByte;
  }
  last_position_ += x;
  rinfo_.data_ = last_position_;
}


void RelocIterator::AdvanceReadData() {
  intptr_t x = 0;
  for (int i = 0; i < kIntptrSize; i++) {
    x |= static_cast<intptr_t>(*--pos_) << i * kBitsPerByte;
  }
  rinfo_.data_ = x;
}


void RelocIterator::AdvanceReadVariableLengthPCJump() {
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


inline int RelocIterator::GetLocatableTypeTag() {
  return *pos_ & ((1 << kLocatableTypeTagBits) - 1);
}


inline void RelocIterator::ReadTaggedId() {
  int8_t signed_b = *pos_;
  // Signed right shift is arithmetic shift.  Tested in test-utils.cc.
  last_id_ += signed_b >> kLocatableTypeTagBits;
  rinfo_.data_ = last_id_;
}


inline void RelocIterator::ReadTaggedPosition() {
  int8_t signed_b = *pos_;
  // Signed right shift is arithmetic shift.  Tested in test-utils.cc.
  last_position_ += signed_b >> kLocatableTypeTagBits;
  rinfo_.data_ = last_position_;
}


static inline RelocInfo::Mode GetPositionModeFromTag(int tag) {
  DCHECK(tag == kNonstatementPositionTag ||
         tag == kStatementPositionTag);
  return (tag == kNonstatementPositionTag) ?
         RelocInfo::POSITION :
         RelocInfo::STATEMENT_POSITION;
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
      ReadTaggedPC();
      if (SetMode(RelocInfo::EMBEDDED_OBJECT)) return;
    } else if (tag == kCodeTargetTag) {
      ReadTaggedPC();
      if (SetMode(RelocInfo::CODE_TARGET)) return;
    } else if (tag == kLocatableTag) {
      ReadTaggedPC();
      Advance();
      int locatable_tag = GetLocatableTypeTag();
      if (locatable_tag == kCodeWithIdTag) {
        if (SetMode(RelocInfo::CODE_TARGET_WITH_ID)) {
          ReadTaggedId();
          return;
        }
      } else {
        // Compact encoding is never used for comments,
        // so it must be a position.
        DCHECK(locatable_tag == kNonstatementPositionTag ||
               locatable_tag == kStatementPositionTag);
        if (mode_mask_ & RelocInfo::kPositionMask) {
          ReadTaggedPosition();
          if (SetMode(GetPositionModeFromTag(locatable_tag))) return;
        }
      }
    } else {
      DCHECK(tag == kDefaultTag);
      int extra_tag = GetExtraTag();
      if (extra_tag == kPCJumpExtraTag) {
        if (GetTopTag() == kVariableLengthPCJumpTopTag) {
          AdvanceReadVariableLengthPCJump();
        } else {
          AdvanceReadPC();
        }
      } else if (extra_tag == kDataJumpExtraTag) {
        int locatable_tag = GetTopTag();
        if (locatable_tag == kCodeWithIdTag) {
          if (SetMode(RelocInfo::CODE_TARGET_WITH_ID)) {
            AdvanceReadId();
            return;
          }
          Advance(kIntSize);
        } else if (locatable_tag != kCommentTag) {
          DCHECK(locatable_tag == kNonstatementPositionTag ||
                 locatable_tag == kStatementPositionTag);
          if (mode_mask_ & RelocInfo::kPositionMask) {
            AdvanceReadPosition();
            if (SetMode(GetPositionModeFromTag(locatable_tag))) return;
          } else {
            Advance(kIntSize);
          }
        } else {
          DCHECK(locatable_tag == kCommentTag);
          if (SetMode(RelocInfo::COMMENT)) {
            AdvanceReadData();
            return;
          }
          Advance(kIntptrSize);
        }
      } else if (extra_tag == kPoolExtraTag) {
        int pool_type = GetTopTag();
        DCHECK(pool_type == kConstPoolTag || pool_type == kVeneerPoolTag);
        RelocInfo::Mode rmode = (pool_type == kConstPoolTag) ?
          RelocInfo::CONST_POOL : RelocInfo::VENEER_POOL;
        if (SetMode(rmode)) {
          AdvanceReadPoolData();
          return;
        }
        Advance(kIntSize);
      } else {
        AdvanceReadPC();
        int rmode = extra_tag + RelocInfo::LAST_COMPACT_ENUM;
        if (SetMode(static_cast<RelocInfo::Mode>(rmode))) return;
      }
    }
  }
  if (code_age_sequence_ != NULL) {
    byte* old_code_age_sequence = code_age_sequence_;
    code_age_sequence_ = NULL;
    if (SetMode(RelocInfo::CODE_AGE_SEQUENCE)) {
      rinfo_.data_ = 0;
      rinfo_.pc_ = old_code_age_sequence;
      return;
    }
  }
  done_ = true;
}


RelocIterator::RelocIterator(Code* code, int mode_mask) {
  rinfo_.host_ = code;
  rinfo_.pc_ = code->instruction_start();
  rinfo_.data_ = 0;
  // Relocation info is read backwards.
  pos_ = code->relocation_start() + code->relocation_size();
  end_ = code->relocation_start();
  done_ = false;
  mode_mask_ = mode_mask;
  last_id_ = 0;
  last_position_ = 0;
  byte* sequence = code->FindCodeAgeSequence();
  // We get the isolate from the map, because at serialization time
  // the code pointer has been cloned and isn't really in heap space.
  Isolate* isolate = code->map()->GetIsolate();
  if (sequence != NULL && !Code::IsYoungSequence(isolate, sequence)) {
    code_age_sequence_ = sequence;
  } else {
    code_age_sequence_ = NULL;
  }
  if (mode_mask_ == 0) pos_ = end_;
  next();
}


RelocIterator::RelocIterator(const CodeDesc& desc, int mode_mask) {
  rinfo_.pc_ = desc.buffer;
  rinfo_.data_ = 0;
  // Relocation info is read backwards.
  pos_ = desc.buffer + desc.buffer_size;
  end_ = pos_ - desc.reloc_size;
  done_ = false;
  mode_mask_ = mode_mask;
  last_id_ = 0;
  last_position_ = 0;
  code_age_sequence_ = NULL;
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
                  RelocInfo::ModeMask(RelocInfo::CELL) |
                  RelocInfo::kApplyMask;
  RelocIterator it(desc, mode_mask);
  return !it.done();
}
#endif


#ifdef ENABLE_DISASSEMBLER
const char* RelocInfo::RelocModeName(RelocInfo::Mode rmode) {
  switch (rmode) {
    case RelocInfo::NONE32:
      return "no reloc 32";
    case RelocInfo::NONE64:
      return "no reloc 64";
    case RelocInfo::EMBEDDED_OBJECT:
      return "embedded object";
    case RelocInfo::CONSTRUCT_CALL:
      return "code target (js construct call)";
    case RelocInfo::DEBUG_BREAK:
      return "debug break";
    case RelocInfo::CODE_TARGET:
      return "code target";
    case RelocInfo::CODE_TARGET_WITH_ID:
      return "code target with id";
    case RelocInfo::CELL:
      return "property cell";
    case RelocInfo::RUNTIME_ENTRY:
      return "runtime entry";
    case RelocInfo::JS_RETURN:
      return "js return";
    case RelocInfo::COMMENT:
      return "comment";
    case RelocInfo::POSITION:
      return "position";
    case RelocInfo::STATEMENT_POSITION:
      return "statement position";
    case RelocInfo::EXTERNAL_REFERENCE:
      return "external reference";
    case RelocInfo::INTERNAL_REFERENCE:
      return "internal reference";
    case RelocInfo::CONST_POOL:
      return "constant pool";
    case RelocInfo::VENEER_POOL:
      return "veneer pool";
    case RelocInfo::DEBUG_BREAK_SLOT:
      return "debug break slot";
    case RelocInfo::CODE_AGE_SEQUENCE:
      return "code_age_sequence";
    case RelocInfo::NUMBER_OF_MODES:
      UNREACHABLE();
      return "number_of_modes";
  }
  return "unknown relocation type";
}


void RelocInfo::Print(Isolate* isolate, OStream& os) {  // NOLINT
  os << pc_ << "  " << RelocModeName(rmode_);
  if (IsComment(rmode_)) {
    os << "  (" << reinterpret_cast<char*>(data_) << ")";
  } else if (rmode_ == EMBEDDED_OBJECT) {
    os << "  (" << Brief(target_object()) << ")";
  } else if (rmode_ == EXTERNAL_REFERENCE) {
    ExternalReferenceEncoder ref_encoder(isolate);
    os << " (" << ref_encoder.NameOfAddress(target_reference()) << ")  ("
       << target_reference() << ")";
  } else if (IsCodeTarget(rmode_)) {
    Code* code = Code::GetCodeFromTargetAddress(target_address());
    os << " (" << Code::Kind2String(code->kind()) << ")  (" << target_address()
       << ")";
    if (rmode_ == CODE_TARGET_WITH_ID) {
      os << " (id=" << static_cast<int>(data_) << ")";
    }
  } else if (IsPosition(rmode_)) {
    os << "  (" << data() << ")";
  } else if (IsRuntimeEntry(rmode_) &&
             isolate->deoptimizer_data() != NULL) {
    // Depotimization bailouts are stored as runtime entries.
    int id = Deoptimizer::GetDeoptimizationId(
        isolate, target_address(), Deoptimizer::EAGER);
    if (id != Deoptimizer::kNotDeoptimizationEntry) {
      os << "  (deoptimization bailout " << id << ")";
    }
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
    case CELL:
      Object::VerifyPointer(target_cell());
      break;
    case DEBUG_BREAK:
    case CONSTRUCT_CALL:
    case CODE_TARGET_WITH_ID:
    case CODE_TARGET: {
      // convert inline target address to code object
      Address addr = target_address();
      CHECK(addr != NULL);
      // Check that we can find the right code object.
      Code* code = Code::GetCodeFromTargetAddress(addr);
      Object* found = isolate->FindCodeObject(addr);
      CHECK(found->IsCode());
      CHECK(code->address() == HeapObject::cast(found)->address());
      break;
    }
    case RUNTIME_ENTRY:
    case JS_RETURN:
    case COMMENT:
    case POSITION:
    case STATEMENT_POSITION:
    case EXTERNAL_REFERENCE:
    case INTERNAL_REFERENCE:
    case CONST_POOL:
    case VENEER_POOL:
    case DEBUG_BREAK_SLOT:
    case NONE32:
    case NONE64:
      break;
    case NUMBER_OF_MODES:
      UNREACHABLE();
      break;
    case CODE_AGE_SEQUENCE:
      DCHECK(Code::IsYoungSequence(isolate, pc_) || code_age_stub()->IsCode());
      break;
  }
}
#endif  // VERIFY_HEAP


// -----------------------------------------------------------------------------
// Implementation of ExternalReference

void ExternalReference::SetUp() {
  double_constants.min_int = kMinInt;
  double_constants.one_half = 0.5;
  double_constants.minus_one_half = -0.5;
  double_constants.canonical_non_hole_nan = base::OS::nan_value();
  double_constants.the_hole_nan = bit_cast<double>(kHoleNanInt64);
  double_constants.negative_infinity = -V8_INFINITY;
  double_constants.uint32_bias =
    static_cast<double>(static_cast<uint32_t>(0xFFFFFFFF)) + 1;

  math_exp_data_mutex = new base::Mutex();
}


void ExternalReference::InitializeMathExpData() {
  // Early return?
  if (math_exp_data_initialized) return;

  base::LockGuard<base::Mutex> lock_guard(math_exp_data_mutex);
  if (!math_exp_data_initialized) {
    // If this is changed, generated code must be adapted too.
    const int kTableSizeBits = 11;
    const int kTableSize = 1 << kTableSizeBits;
    const double kTableSizeDouble = static_cast<double>(kTableSize);

    math_exp_constants_array = new double[9];
    // Input values smaller than this always return 0.
    math_exp_constants_array[0] = -708.39641853226408;
    // Input values larger than this always return +Infinity.
    math_exp_constants_array[1] = 709.78271289338397;
    math_exp_constants_array[2] = V8_INFINITY;
    // The rest is black magic. Do not attempt to understand it. It is
    // loosely based on the "expd" function published at:
    // http://herumi.blogspot.com/2011/08/fast-double-precision-exponential.html
    const double constant3 = (1 << kTableSizeBits) / std::log(2.0);
    math_exp_constants_array[3] = constant3;
    math_exp_constants_array[4] =
        static_cast<double>(static_cast<int64_t>(3) << 51);
    math_exp_constants_array[5] = 1 / constant3;
    math_exp_constants_array[6] = 3.0000000027955394;
    math_exp_constants_array[7] = 0.16666666685227835;
    math_exp_constants_array[8] = 1;

    math_exp_log_table_array = new double[kTableSize];
    for (int i = 0; i < kTableSize; i++) {
      double value = std::pow(2, i / kTableSizeDouble);
      uint64_t bits = bit_cast<uint64_t, double>(value);
      bits &= (static_cast<uint64_t>(1) << 52) - 1;
      double mantissa = bit_cast<double, uint64_t>(bits);
      math_exp_log_table_array[i] = mantissa;
    }

    math_exp_data_initialized = true;
  }
}


void ExternalReference::TearDownMathExpData() {
  delete[] math_exp_constants_array;
  math_exp_constants_array = NULL;
  delete[] math_exp_log_table_array;
  math_exp_log_table_array = NULL;
  delete math_exp_data_mutex;
  math_exp_data_mutex = NULL;
}


ExternalReference::ExternalReference(Builtins::CFunctionId id, Isolate* isolate)
  : address_(Redirect(isolate, Builtins::c_function_address(id))) {}


ExternalReference::ExternalReference(
    ApiFunction* fun,
    Type type = ExternalReference::BUILTIN_CALL,
    Isolate* isolate = NULL)
  : address_(Redirect(isolate, fun->address(), type)) {}


ExternalReference::ExternalReference(Builtins::Name name, Isolate* isolate)
  : address_(isolate->builtins()->builtin_address(name)) {}


ExternalReference::ExternalReference(Runtime::FunctionId id,
                                     Isolate* isolate)
  : address_(Redirect(isolate, Runtime::FunctionForId(id)->entry)) {}


ExternalReference::ExternalReference(const Runtime::Function* f,
                                     Isolate* isolate)
  : address_(Redirect(isolate, f->entry)) {}


ExternalReference ExternalReference::isolate_address(Isolate* isolate) {
  return ExternalReference(isolate);
}


ExternalReference::ExternalReference(const IC_Utility& ic_utility,
                                     Isolate* isolate)
  : address_(Redirect(isolate, ic_utility.address())) {}


ExternalReference::ExternalReference(StatsCounter* counter)
  : address_(reinterpret_cast<Address>(counter->GetInternalPointer())) {}


ExternalReference::ExternalReference(Isolate::AddressId id, Isolate* isolate)
  : address_(isolate->get_address_from_id(id)) {}


ExternalReference::ExternalReference(const SCTableReference& table_ref)
  : address_(table_ref.address()) {}


ExternalReference ExternalReference::
    incremental_marking_record_write_function(Isolate* isolate) {
  return ExternalReference(Redirect(
      isolate,
      FUNCTION_ADDR(IncrementalMarking::RecordWriteFromCode)));
}


ExternalReference ExternalReference::
    store_buffer_overflow_function(Isolate* isolate) {
  return ExternalReference(Redirect(
      isolate,
      FUNCTION_ADDR(StoreBuffer::StoreBufferOverflow)));
}


ExternalReference ExternalReference::flush_icache_function(Isolate* isolate) {
  return ExternalReference(
      Redirect(isolate, FUNCTION_ADDR(CpuFeatures::FlushICache)));
}


ExternalReference ExternalReference::delete_handle_scope_extensions(
    Isolate* isolate) {
  return ExternalReference(Redirect(
      isolate,
      FUNCTION_ADDR(HandleScope::DeleteExtensions)));
}


ExternalReference ExternalReference::get_date_field_function(
    Isolate* isolate) {
  return ExternalReference(Redirect(isolate, FUNCTION_ADDR(JSDate::GetField)));
}


ExternalReference ExternalReference::get_make_code_young_function(
    Isolate* isolate) {
  return ExternalReference(Redirect(
      isolate, FUNCTION_ADDR(Code::MakeCodeAgeSequenceYoung)));
}


ExternalReference ExternalReference::get_mark_code_as_executed_function(
    Isolate* isolate) {
  return ExternalReference(Redirect(
      isolate, FUNCTION_ADDR(Code::MarkCodeAsExecuted)));
}


ExternalReference ExternalReference::date_cache_stamp(Isolate* isolate) {
  return ExternalReference(isolate->date_cache()->stamp_address());
}


ExternalReference ExternalReference::stress_deopt_count(Isolate* isolate) {
  return ExternalReference(isolate->stress_deopt_count_address());
}


ExternalReference ExternalReference::new_deoptimizer_function(
    Isolate* isolate) {
  return ExternalReference(
      Redirect(isolate, FUNCTION_ADDR(Deoptimizer::New)));
}


ExternalReference ExternalReference::compute_output_frames_function(
    Isolate* isolate) {
  return ExternalReference(
      Redirect(isolate, FUNCTION_ADDR(Deoptimizer::ComputeOutputFrames)));
}


ExternalReference ExternalReference::log_enter_external_function(
    Isolate* isolate) {
  return ExternalReference(
      Redirect(isolate, FUNCTION_ADDR(Logger::EnterExternal)));
}


ExternalReference ExternalReference::log_leave_external_function(
    Isolate* isolate) {
  return ExternalReference(
      Redirect(isolate, FUNCTION_ADDR(Logger::LeaveExternal)));
}


ExternalReference ExternalReference::keyed_lookup_cache_keys(Isolate* isolate) {
  return ExternalReference(isolate->keyed_lookup_cache()->keys_address());
}


ExternalReference ExternalReference::keyed_lookup_cache_field_offsets(
    Isolate* isolate) {
  return ExternalReference(
      isolate->keyed_lookup_cache()->field_offsets_address());
}


ExternalReference ExternalReference::roots_array_start(Isolate* isolate) {
  return ExternalReference(isolate->heap()->roots_array_start());
}


ExternalReference ExternalReference::allocation_sites_list_address(
    Isolate* isolate) {
  return ExternalReference(isolate->heap()->allocation_sites_list_address());
}


ExternalReference ExternalReference::address_of_stack_limit(Isolate* isolate) {
  return ExternalReference(isolate->stack_guard()->address_of_jslimit());
}


ExternalReference ExternalReference::address_of_real_stack_limit(
    Isolate* isolate) {
  return ExternalReference(isolate->stack_guard()->address_of_real_jslimit());
}


ExternalReference ExternalReference::address_of_regexp_stack_limit(
    Isolate* isolate) {
  return ExternalReference(isolate->regexp_stack()->limit_address());
}


ExternalReference ExternalReference::new_space_start(Isolate* isolate) {
  return ExternalReference(isolate->heap()->NewSpaceStart());
}


ExternalReference ExternalReference::store_buffer_top(Isolate* isolate) {
  return ExternalReference(isolate->heap()->store_buffer()->TopAddress());
}


ExternalReference ExternalReference::new_space_mask(Isolate* isolate) {
  return ExternalReference(reinterpret_cast<Address>(
      isolate->heap()->NewSpaceMask()));
}


ExternalReference ExternalReference::new_space_allocation_top_address(
    Isolate* isolate) {
  return ExternalReference(isolate->heap()->NewSpaceAllocationTopAddress());
}


ExternalReference ExternalReference::new_space_allocation_limit_address(
    Isolate* isolate) {
  return ExternalReference(isolate->heap()->NewSpaceAllocationLimitAddress());
}


ExternalReference ExternalReference::old_pointer_space_allocation_top_address(
    Isolate* isolate) {
  return ExternalReference(
      isolate->heap()->OldPointerSpaceAllocationTopAddress());
}


ExternalReference ExternalReference::old_pointer_space_allocation_limit_address(
    Isolate* isolate) {
  return ExternalReference(
      isolate->heap()->OldPointerSpaceAllocationLimitAddress());
}


ExternalReference ExternalReference::old_data_space_allocation_top_address(
    Isolate* isolate) {
  return ExternalReference(
      isolate->heap()->OldDataSpaceAllocationTopAddress());
}


ExternalReference ExternalReference::old_data_space_allocation_limit_address(
    Isolate* isolate) {
  return ExternalReference(
      isolate->heap()->OldDataSpaceAllocationLimitAddress());
}


ExternalReference ExternalReference::handle_scope_level_address(
    Isolate* isolate) {
  return ExternalReference(HandleScope::current_level_address(isolate));
}


ExternalReference ExternalReference::handle_scope_next_address(
    Isolate* isolate) {
  return ExternalReference(HandleScope::current_next_address(isolate));
}


ExternalReference ExternalReference::handle_scope_limit_address(
    Isolate* isolate) {
  return ExternalReference(HandleScope::current_limit_address(isolate));
}


ExternalReference ExternalReference::scheduled_exception_address(
    Isolate* isolate) {
  return ExternalReference(isolate->scheduled_exception_address());
}


ExternalReference ExternalReference::address_of_pending_message_obj(
    Isolate* isolate) {
  return ExternalReference(isolate->pending_message_obj_address());
}


ExternalReference ExternalReference::address_of_has_pending_message(
    Isolate* isolate) {
  return ExternalReference(isolate->has_pending_message_address());
}


ExternalReference ExternalReference::address_of_pending_message_script(
    Isolate* isolate) {
  return ExternalReference(isolate->pending_message_script_address());
}


ExternalReference ExternalReference::address_of_min_int() {
  return ExternalReference(reinterpret_cast<void*>(&double_constants.min_int));
}


ExternalReference ExternalReference::address_of_one_half() {
  return ExternalReference(reinterpret_cast<void*>(&double_constants.one_half));
}


ExternalReference ExternalReference::address_of_minus_one_half() {
  return ExternalReference(
      reinterpret_cast<void*>(&double_constants.minus_one_half));
}


ExternalReference ExternalReference::address_of_negative_infinity() {
  return ExternalReference(
      reinterpret_cast<void*>(&double_constants.negative_infinity));
}


ExternalReference ExternalReference::address_of_canonical_non_hole_nan() {
  return ExternalReference(
      reinterpret_cast<void*>(&double_constants.canonical_non_hole_nan));
}


ExternalReference ExternalReference::address_of_the_hole_nan() {
  return ExternalReference(
      reinterpret_cast<void*>(&double_constants.the_hole_nan));
}


ExternalReference ExternalReference::address_of_uint32_bias() {
  return ExternalReference(
      reinterpret_cast<void*>(&double_constants.uint32_bias));
}


ExternalReference ExternalReference::is_profiling_address(Isolate* isolate) {
  return ExternalReference(isolate->cpu_profiler()->is_profiling_address());
}


ExternalReference ExternalReference::invoke_function_callback(
    Isolate* isolate) {
  Address thunk_address = FUNCTION_ADDR(&InvokeFunctionCallback);
  ExternalReference::Type thunk_type = ExternalReference::PROFILING_API_CALL;
  ApiFunction thunk_fun(thunk_address);
  return ExternalReference(&thunk_fun, thunk_type, isolate);
}


ExternalReference ExternalReference::invoke_accessor_getter_callback(
    Isolate* isolate) {
  Address thunk_address = FUNCTION_ADDR(&InvokeAccessorGetterCallback);
  ExternalReference::Type thunk_type =
      ExternalReference::PROFILING_GETTER_CALL;
  ApiFunction thunk_fun(thunk_address);
  return ExternalReference(&thunk_fun, thunk_type, isolate);
}


#ifndef V8_INTERPRETED_REGEXP

ExternalReference ExternalReference::re_check_stack_guard_state(
    Isolate* isolate) {
  Address function;
#if V8_TARGET_ARCH_X64
  function = FUNCTION_ADDR(RegExpMacroAssemblerX64::CheckStackGuardState);
#elif V8_TARGET_ARCH_IA32
  function = FUNCTION_ADDR(RegExpMacroAssemblerIA32::CheckStackGuardState);
#elif V8_TARGET_ARCH_ARM64
  function = FUNCTION_ADDR(RegExpMacroAssemblerARM64::CheckStackGuardState);
#elif V8_TARGET_ARCH_ARM
  function = FUNCTION_ADDR(RegExpMacroAssemblerARM::CheckStackGuardState);
#elif V8_TARGET_ARCH_MIPS
  function = FUNCTION_ADDR(RegExpMacroAssemblerMIPS::CheckStackGuardState);
#elif V8_TARGET_ARCH_MIPS64
  function = FUNCTION_ADDR(RegExpMacroAssemblerMIPS::CheckStackGuardState);
#elif V8_TARGET_ARCH_X87
  function = FUNCTION_ADDR(RegExpMacroAssemblerX87::CheckStackGuardState);
#else
  UNREACHABLE();
#endif
  return ExternalReference(Redirect(isolate, function));
}


ExternalReference ExternalReference::re_grow_stack(Isolate* isolate) {
  return ExternalReference(
      Redirect(isolate, FUNCTION_ADDR(NativeRegExpMacroAssembler::GrowStack)));
}

ExternalReference ExternalReference::re_case_insensitive_compare_uc16(
    Isolate* isolate) {
  return ExternalReference(Redirect(
      isolate,
      FUNCTION_ADDR(NativeRegExpMacroAssembler::CaseInsensitiveCompareUC16)));
}


ExternalReference ExternalReference::re_word_character_map() {
  return ExternalReference(
      NativeRegExpMacroAssembler::word_character_map_address());
}

ExternalReference ExternalReference::address_of_static_offsets_vector(
    Isolate* isolate) {
  return ExternalReference(
      reinterpret_cast<Address>(isolate->jsregexp_static_offsets_vector()));
}

ExternalReference ExternalReference::address_of_regexp_stack_memory_address(
    Isolate* isolate) {
  return ExternalReference(
      isolate->regexp_stack()->memory_address());
}

ExternalReference ExternalReference::address_of_regexp_stack_memory_size(
    Isolate* isolate) {
  return ExternalReference(isolate->regexp_stack()->memory_size_address());
}

#endif  // V8_INTERPRETED_REGEXP


ExternalReference ExternalReference::math_log_double_function(
    Isolate* isolate) {
  typedef double (*d2d)(double x);
  return ExternalReference(Redirect(isolate,
                                    FUNCTION_ADDR(static_cast<d2d>(std::log)),
                                    BUILTIN_FP_CALL));
}


ExternalReference ExternalReference::math_exp_constants(int constant_index) {
  DCHECK(math_exp_data_initialized);
  return ExternalReference(
      reinterpret_cast<void*>(math_exp_constants_array + constant_index));
}


ExternalReference ExternalReference::math_exp_log_table() {
  DCHECK(math_exp_data_initialized);
  return ExternalReference(reinterpret_cast<void*>(math_exp_log_table_array));
}


ExternalReference ExternalReference::page_flags(Page* page) {
  return ExternalReference(reinterpret_cast<Address>(page) +
                           MemoryChunk::kFlagsOffset);
}


ExternalReference ExternalReference::ForDeoptEntry(Address entry) {
  return ExternalReference(entry);
}


ExternalReference ExternalReference::cpu_features() {
  DCHECK(CpuFeatures::initialized_);
  return ExternalReference(&CpuFeatures::supported_);
}


ExternalReference ExternalReference::debug_is_active_address(
    Isolate* isolate) {
  return ExternalReference(isolate->debug()->is_active_address());
}


ExternalReference ExternalReference::debug_after_break_target_address(
    Isolate* isolate) {
  return ExternalReference(isolate->debug()->after_break_target_address());
}


ExternalReference
    ExternalReference::debug_restarter_frame_function_pointer_address(
        Isolate* isolate) {
  return ExternalReference(
      isolate->debug()->restarter_frame_function_pointer_address());
}


double power_helper(double x, double y) {
  int y_int = static_cast<int>(y);
  if (y == y_int) {
    return power_double_int(x, y_int);  // Returns 1 if exponent is 0.
  }
  if (y == 0.5) {
    return (std::isinf(x)) ? V8_INFINITY
                           : fast_sqrt(x + 0.0);  // Convert -0 to +0.
  }
  if (y == -0.5) {
    return (std::isinf(x)) ? 0 : 1.0 / fast_sqrt(x + 0.0);  // Convert -0 to +0.
  }
  return power_double_double(x, y);
}


// Helper function to compute x^y, where y is known to be an
// integer. Uses binary decomposition to limit the number of
// multiplications; see the discussion in "Hacker's Delight" by Henry
// S. Warren, Jr., figure 11-6, page 213.
double power_double_int(double x, int y) {
  double m = (y < 0) ? 1 / x : x;
  unsigned n = (y < 0) ? -y : y;
  double p = 1;
  while (n != 0) {
    if ((n & 1) != 0) p *= m;
    m *= m;
    if ((n & 2) != 0) p *= m;
    m *= m;
    n >>= 2;
  }
  return p;
}


double power_double_double(double x, double y) {
#if defined(__MINGW64_VERSION_MAJOR) && \
    (!defined(__MINGW64_VERSION_RC) || __MINGW64_VERSION_RC < 1)
  // MinGW64 has a custom implementation for pow.  This handles certain
  // special cases that are different.
  if ((x == 0.0 || std::isinf(x)) && std::isfinite(y)) {
    double f;
    if (std::modf(y, &f) != 0.0) {
      return ((x == 0.0) ^ (y > 0)) ? V8_INFINITY : 0;
    }
  }

  if (x == 2.0) {
    int y_int = static_cast<int>(y);
    if (y == y_int) {
      return std::ldexp(1.0, y_int);
    }
  }
#endif

  // The checks for special cases can be dropped in ia32 because it has already
  // been done in generated code before bailing out here.
  if (std::isnan(y) || ((x == 1 || x == -1) && std::isinf(y))) {
    return base::OS::nan_value();
  }
  return std::pow(x, y);
}


ExternalReference ExternalReference::power_double_double_function(
    Isolate* isolate) {
  return ExternalReference(Redirect(isolate,
                                    FUNCTION_ADDR(power_double_double),
                                    BUILTIN_FP_FP_CALL));
}


ExternalReference ExternalReference::power_double_int_function(
    Isolate* isolate) {
  return ExternalReference(Redirect(isolate,
                                    FUNCTION_ADDR(power_double_int),
                                    BUILTIN_FP_INT_CALL));
}


bool EvalComparison(Token::Value op, double op1, double op2) {
  DCHECK(Token::IsCompareOp(op));
  switch (op) {
    case Token::EQ:
    case Token::EQ_STRICT: return (op1 == op2);
    case Token::NE: return (op1 != op2);
    case Token::LT: return (op1 < op2);
    case Token::GT: return (op1 > op2);
    case Token::LTE: return (op1 <= op2);
    case Token::GTE: return (op1 >= op2);
    default:
      UNREACHABLE();
      return false;
  }
}


ExternalReference ExternalReference::mod_two_doubles_operation(
    Isolate* isolate) {
  return ExternalReference(Redirect(isolate,
                                    FUNCTION_ADDR(modulo),
                                    BUILTIN_FP_FP_CALL));
}


ExternalReference ExternalReference::debug_break(Isolate* isolate) {
  return ExternalReference(Redirect(isolate, FUNCTION_ADDR(Debug_Break)));
}


ExternalReference ExternalReference::debug_step_in_fp_address(
    Isolate* isolate) {
  return ExternalReference(isolate->debug()->step_in_fp_addr());
}


void PositionsRecorder::RecordPosition(int pos) {
  DCHECK(pos != RelocInfo::kNoPosition);
  DCHECK(pos >= 0);
  state_.current_position = pos;
  LOG_CODE_EVENT(assembler_->isolate(),
                 CodeLinePosInfoAddPositionEvent(jit_handler_data_,
                                                 assembler_->pc_offset(),
                                                 pos));
}


void PositionsRecorder::RecordStatementPosition(int pos) {
  DCHECK(pos != RelocInfo::kNoPosition);
  DCHECK(pos >= 0);
  state_.current_statement_position = pos;
  LOG_CODE_EVENT(assembler_->isolate(),
                 CodeLinePosInfoAddStatementPositionEvent(
                     jit_handler_data_,
                     assembler_->pc_offset(),
                     pos));
}


bool PositionsRecorder::WriteRecordedPositions() {
  bool written = false;

  // Write the statement position if it is different from what was written last
  // time.
  if (state_.current_statement_position != state_.written_statement_position) {
    EnsureSpace ensure_space(assembler_);
    assembler_->RecordRelocInfo(RelocInfo::STATEMENT_POSITION,
                                state_.current_statement_position);
    state_.written_statement_position = state_.current_statement_position;
    written = true;
  }

  // Write the position if it is different from what was written last time and
  // also different from the written statement position.
  if (state_.current_position != state_.written_position &&
      state_.current_position != state_.written_statement_position) {
    EnsureSpace ensure_space(assembler_);
    assembler_->RecordRelocInfo(RelocInfo::POSITION, state_.current_position);
    state_.written_position = state_.current_position;
    written = true;
  }

  // Return whether something was written.
  return written;
}

} }  // namespace v8::internal
