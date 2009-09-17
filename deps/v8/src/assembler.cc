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
// Copyright 2006-2009 the V8 project authors. All rights reserved.

#include "v8.h"

#include "arguments.h"
#include "execution.h"
#include "ic-inl.h"
#include "factory.h"
#include "runtime.h"
#include "serialize.h"
#include "stub-cache.h"
#include "regexp-stack.h"
#include "ast.h"
#include "regexp-macro-assembler.h"
// Include native regexp-macro-assembler.
#ifdef V8_NATIVE_REGEXP
#if V8_TARGET_ARCH_IA32
#include "ia32/regexp-macro-assembler-ia32.h"
#elif V8_TARGET_ARCH_X64
#include "x64/regexp-macro-assembler-x64.h"
#elif V8_TARGET_ARCH_ARM
#include "arm/regexp-macro-assembler-arm.h"
#else  // Unknown architecture.
#error "Unknown architecture."
#endif  // Target architecture.
#endif  // V8_NATIVE_REGEXP

namespace v8 {
namespace internal {


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
// Encoding
//
// The most common modes are given single-byte encodings.  Also, it is
// easy to identify the type of reloc info and skip unwanted modes in
// an iteration.
//
// The encoding relies on the fact that there are less than 14
// different relocation modes.
//
// embedded_object:    [6 bits pc delta] 00
//
// code_taget:         [6 bits pc delta] 01
//
// position:           [6 bits pc delta] 10,
//                     [7 bits signed data delta] 0
//
// statement_position: [6 bits pc delta] 10,
//                     [7 bits signed data delta] 1
//
// any nondata mode:   00 [4 bits rmode] 11,  // rmode: 0..13 only
//                     00 [6 bits pc delta]
//
// pc-jump:            00 1111 11,
//                     00 [6 bits pc delta]
//
// pc-jump:            01 1111 11,
// (variable length)   7 - 26 bit pc delta, written in chunks of 7
//                     bits, the lowest 7 bits written first.
//
// data-jump + pos:    00 1110 11,
//                     signed intptr_t, lowest byte written first
//
// data-jump + st.pos: 01 1110 11,
//                     signed intptr_t, lowest byte written first
//
// data-jump + comm.:  10 1110 11,
//                     signed intptr_t, lowest byte written first
//
const int kMaxRelocModes = 14;

const int kTagBits = 2;
const int kTagMask = (1 << kTagBits) - 1;
const int kExtraTagBits = 4;
const int kPositionTypeTagBits = 1;
const int kSmallDataBits = kBitsPerByte - kPositionTypeTagBits;

const int kEmbeddedObjectTag = 0;
const int kCodeTargetTag = 1;
const int kPositionTag = 2;
const int kDefaultTag = 3;

const int kPCJumpTag = (1 << kExtraTagBits) - 1;

const int kSmallPCDeltaBits = kBitsPerByte - kTagBits;
const int kSmallPCDeltaMask = (1 << kSmallPCDeltaBits) - 1;

const int kVariableLengthPCJumpTopTag = 1;
const int kChunkBits = 7;
const int kChunkMask = (1 << kChunkBits) - 1;
const int kLastChunkTagBits = 1;
const int kLastChunkTagMask = 1;
const int kLastChunkTag = 1;


const int kDataJumpTag = kPCJumpTag - 1;

const int kNonstatementPositionTag = 0;
const int kStatementPositionTag = 1;
const int kCommentTag = 2;


uint32_t RelocInfoWriter::WriteVariableLengthPCJump(uint32_t pc_delta) {
  // Return if the pc_delta can fit in kSmallPCDeltaBits bits.
  // Otherwise write a variable length PC jump for the bits that do
  // not fit in the kSmallPCDeltaBits bits.
  if (is_uintn(pc_delta, kSmallPCDeltaBits)) return pc_delta;
  WriteExtraTag(kPCJumpTag, kVariableLengthPCJumpTopTag);
  uint32_t pc_jump = pc_delta >> kSmallPCDeltaBits;
  ASSERT(pc_jump > 0);
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
  *--pos_ = data_delta << kPositionTypeTagBits | tag;
}


void RelocInfoWriter::WriteExtraTag(int extra_tag, int top_tag) {
  *--pos_ = top_tag << (kTagBits + kExtraTagBits) |
            extra_tag << kTagBits |
            kDefaultTag;
}


void RelocInfoWriter::WriteExtraTaggedPC(uint32_t pc_delta, int extra_tag) {
  // Write two-byte tagged pc-delta, possibly preceded by var. length pc-jump.
  pc_delta = WriteVariableLengthPCJump(pc_delta);
  WriteExtraTag(extra_tag, 0);
  *--pos_ = pc_delta;
}


void RelocInfoWriter::WriteExtraTaggedData(intptr_t data_delta, int top_tag) {
  WriteExtraTag(kDataJumpTag, top_tag);
  for (int i = 0; i < kIntptrSize; i++) {
    *--pos_ = data_delta;
  // Signed right shift is arithmetic shift.  Tested in test-utils.cc.
    data_delta = data_delta >> kBitsPerByte;
  }
}


void RelocInfoWriter::Write(const RelocInfo* rinfo) {
#ifdef DEBUG
  byte* begin_pos = pos_;
#endif
  Counters::reloc_info_count.Increment();
  ASSERT(rinfo->pc() - last_pc_ >= 0);
  ASSERT(RelocInfo::NUMBER_OF_MODES < kMaxRelocModes);
  // Use unsigned delta-encoding for pc.
  uint32_t pc_delta = rinfo->pc() - last_pc_;
  RelocInfo::Mode rmode = rinfo->rmode();

  // The two most common modes are given small tags, and usually fit in a byte.
  if (rmode == RelocInfo::EMBEDDED_OBJECT) {
    WriteTaggedPC(pc_delta, kEmbeddedObjectTag);
  } else if (rmode == RelocInfo::CODE_TARGET) {
    WriteTaggedPC(pc_delta, kCodeTargetTag);
  } else if (RelocInfo::IsPosition(rmode)) {
    // Use signed delta-encoding for data.
    intptr_t data_delta = rinfo->data() - last_data_;
    int pos_type_tag = rmode == RelocInfo::POSITION ? kNonstatementPositionTag
                                                    : kStatementPositionTag;
    // Check if data is small enough to fit in a tagged byte.
    // We cannot use is_intn because data_delta is not an int32_t.
    if (data_delta >= -(1 << (kSmallDataBits-1)) &&
        data_delta < 1 << (kSmallDataBits-1)) {
      WriteTaggedPC(pc_delta, kPositionTag);
      WriteTaggedData(data_delta, pos_type_tag);
      last_data_ = rinfo->data();
    } else {
      // Otherwise, use costly encoding.
      WriteExtraTaggedPC(pc_delta, kPCJumpTag);
      WriteExtraTaggedData(data_delta, pos_type_tag);
      last_data_ = rinfo->data();
    }
  } else if (RelocInfo::IsComment(rmode)) {
    // Comments are normally not generated, so we use the costly encoding.
    WriteExtraTaggedPC(pc_delta, kPCJumpTag);
    WriteExtraTaggedData(rinfo->data() - last_data_, kCommentTag);
    last_data_ = rinfo->data();
  } else {
    // For all other modes we simply use the mode as the extra tag.
    // None of these modes need a data component.
    ASSERT(rmode < kPCJumpTag && rmode < kDataJumpTag);
    WriteExtraTaggedPC(pc_delta, rmode);
  }
  last_pc_ = rinfo->pc();
#ifdef DEBUG
  ASSERT(begin_pos - pos_ <= kMaxSize);
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


void RelocIterator::AdvanceReadData() {
  intptr_t x = 0;
  for (int i = 0; i < kIntptrSize; i++) {
    x |= static_cast<intptr_t>(*--pos_) << i * kBitsPerByte;
  }
  rinfo_.data_ += x;
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


inline int RelocIterator::GetPositionTypeTag() {
  return *pos_ & ((1 << kPositionTypeTagBits) - 1);
}


inline void RelocIterator::ReadTaggedData() {
  int8_t signed_b = *pos_;
  // Signed right shift is arithmetic shift.  Tested in test-utils.cc.
  rinfo_.data_ += signed_b >> kPositionTypeTagBits;
}


inline RelocInfo::Mode RelocIterator::DebugInfoModeFromTag(int tag) {
  if (tag == kStatementPositionTag) {
    return RelocInfo::STATEMENT_POSITION;
  } else if (tag == kNonstatementPositionTag) {
    return RelocInfo::POSITION;
  } else {
    ASSERT(tag == kCommentTag);
    return RelocInfo::COMMENT;
  }
}


void RelocIterator::next() {
  ASSERT(!done());
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
      if (*(reinterpret_cast<int*>(rinfo_.pc())) == 0x61) {
        tag = 0;
      }
      if (SetMode(RelocInfo::CODE_TARGET)) return;
    } else if (tag == kPositionTag) {
      ReadTaggedPC();
      Advance();
      // Check if we want source positions.
      if (mode_mask_ & RelocInfo::kPositionMask) {
        // Check if we want this type of source position.
        if (SetMode(DebugInfoModeFromTag(GetPositionTypeTag()))) {
          // Finally read the data before returning.
          ReadTaggedData();
          return;
        }
      }
    } else {
      ASSERT(tag == kDefaultTag);
      int extra_tag = GetExtraTag();
      if (extra_tag == kPCJumpTag) {
        int top_tag = GetTopTag();
        if (top_tag == kVariableLengthPCJumpTopTag) {
          AdvanceReadVariableLengthPCJump();
        } else {
          AdvanceReadPC();
        }
      } else if (extra_tag == kDataJumpTag) {
        // Check if we want debug modes (the only ones with data).
        if (mode_mask_ & RelocInfo::kDebugMask) {
          int top_tag = GetTopTag();
          AdvanceReadData();
          if (SetMode(DebugInfoModeFromTag(top_tag))) return;
        } else {
          // Otherwise, just skip over the data.
          Advance(kIntptrSize);
        }
      } else {
        AdvanceReadPC();
        if (SetMode(static_cast<RelocInfo::Mode>(extra_tag))) return;
      }
    }
  }
  done_ = true;
}


RelocIterator::RelocIterator(Code* code, int mode_mask) {
  rinfo_.pc_ = code->instruction_start();
  rinfo_.data_ = 0;
  // relocation info is read backwards
  pos_ = code->relocation_start() + code->relocation_size();
  end_ = code->relocation_start();
  done_ = false;
  mode_mask_ = mode_mask;
  if (mode_mask_ == 0) pos_ = end_;
  next();
}


RelocIterator::RelocIterator(const CodeDesc& desc, int mode_mask) {
  rinfo_.pc_ = desc.buffer;
  rinfo_.data_ = 0;
  // relocation info is read backwards
  pos_ = desc.buffer + desc.buffer_size;
  end_ = pos_ - desc.reloc_size;
  done_ = false;
  mode_mask_ = mode_mask;
  if (mode_mask_ == 0) pos_ = end_;
  next();
}


// -----------------------------------------------------------------------------
// Implementation of RelocInfo


#ifdef ENABLE_DISASSEMBLER
const char* RelocInfo::RelocModeName(RelocInfo::Mode rmode) {
  switch (rmode) {
    case RelocInfo::NONE:
      return "no reloc";
    case RelocInfo::EMBEDDED_OBJECT:
      return "embedded object";
    case RelocInfo::EMBEDDED_STRING:
      return "embedded string";
    case RelocInfo::CONSTRUCT_CALL:
      return "code target (js construct call)";
    case RelocInfo::CODE_TARGET_CONTEXT:
      return "code target (context)";
    case RelocInfo::CODE_TARGET:
      return "code target";
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
    case RelocInfo::NUMBER_OF_MODES:
      UNREACHABLE();
      return "number_of_modes";
  }
  return "unknown relocation type";
}


void RelocInfo::Print() {
  PrintF("%p  %s", pc_, RelocModeName(rmode_));
  if (IsComment(rmode_)) {
    PrintF("  (%s)", data_);
  } else if (rmode_ == EMBEDDED_OBJECT) {
    PrintF("  (");
    target_object()->ShortPrint();
    PrintF(")");
  } else if (rmode_ == EXTERNAL_REFERENCE) {
    ExternalReferenceEncoder ref_encoder;
    PrintF(" (%s)  (%p)",
           ref_encoder.NameOfAddress(*target_reference_address()),
           *target_reference_address());
  } else if (IsCodeTarget(rmode_)) {
    Code* code = Code::GetCodeFromTargetAddress(target_address());
    PrintF(" (%s)  (%p)", Code::Kind2String(code->kind()), target_address());
  } else if (IsPosition(rmode_)) {
    PrintF("  (%d)", data());
  }

  PrintF("\n");
}
#endif  // ENABLE_DISASSEMBLER


#ifdef DEBUG
void RelocInfo::Verify() {
  switch (rmode_) {
    case EMBEDDED_OBJECT:
      Object::VerifyPointer(target_object());
      break;
    case CONSTRUCT_CALL:
    case CODE_TARGET_CONTEXT:
    case CODE_TARGET: {
      // convert inline target address to code object
      Address addr = target_address();
      ASSERT(addr != NULL);
      // Check that we can find the right code object.
      Code* code = Code::GetCodeFromTargetAddress(addr);
      Object* found = Heap::FindCodeObject(addr);
      ASSERT(found->IsCode());
      ASSERT(code->address() == HeapObject::cast(found)->address());
      break;
    }
    case RelocInfo::EMBEDDED_STRING:
    case RUNTIME_ENTRY:
    case JS_RETURN:
    case COMMENT:
    case POSITION:
    case STATEMENT_POSITION:
    case EXTERNAL_REFERENCE:
    case INTERNAL_REFERENCE:
    case NONE:
      break;
    case NUMBER_OF_MODES:
      UNREACHABLE();
      break;
  }
}
#endif  // DEBUG


// -----------------------------------------------------------------------------
// Implementation of ExternalReference

ExternalReference::ExternalReference(Builtins::CFunctionId id)
  : address_(Redirect(Builtins::c_function_address(id))) {}


ExternalReference::ExternalReference(Builtins::Name name)
  : address_(Builtins::builtin_address(name)) {}


ExternalReference::ExternalReference(Runtime::FunctionId id)
  : address_(Redirect(Runtime::FunctionForId(id)->entry)) {}


ExternalReference::ExternalReference(Runtime::Function* f)
  : address_(Redirect(f->entry)) {}


ExternalReference::ExternalReference(const IC_Utility& ic_utility)
  : address_(Redirect(ic_utility.address())) {}

#ifdef ENABLE_DEBUGGER_SUPPORT
ExternalReference::ExternalReference(const Debug_Address& debug_address)
  : address_(debug_address.address()) {}
#endif

ExternalReference::ExternalReference(StatsCounter* counter)
  : address_(reinterpret_cast<Address>(counter->GetInternalPointer())) {}


ExternalReference::ExternalReference(Top::AddressId id)
  : address_(Top::get_address_from_id(id)) {}


ExternalReference::ExternalReference(const SCTableReference& table_ref)
  : address_(table_ref.address()) {}


ExternalReference ExternalReference::perform_gc_function() {
  return ExternalReference(Redirect(FUNCTION_ADDR(Runtime::PerformGC)));
}


ExternalReference ExternalReference::builtin_passed_function() {
  return ExternalReference(&Builtins::builtin_passed_function);
}


ExternalReference ExternalReference::random_positive_smi_function() {
  return ExternalReference(Redirect(FUNCTION_ADDR(V8::RandomPositiveSmi)));
}


ExternalReference ExternalReference::the_hole_value_location() {
  return ExternalReference(Factory::the_hole_value().location());
}


ExternalReference ExternalReference::roots_address() {
  return ExternalReference(Heap::roots_address());
}


ExternalReference ExternalReference::address_of_stack_guard_limit() {
  return ExternalReference(StackGuard::address_of_jslimit());
}


ExternalReference ExternalReference::address_of_regexp_stack_limit() {
  return ExternalReference(RegExpStack::limit_address());
}


ExternalReference ExternalReference::new_space_start() {
  return ExternalReference(Heap::NewSpaceStart());
}


ExternalReference ExternalReference::new_space_allocation_top_address() {
  return ExternalReference(Heap::NewSpaceAllocationTopAddress());
}


ExternalReference ExternalReference::heap_always_allocate_scope_depth() {
  return ExternalReference(Heap::always_allocate_scope_depth_address());
}


ExternalReference ExternalReference::new_space_allocation_limit_address() {
  return ExternalReference(Heap::NewSpaceAllocationLimitAddress());
}

#ifdef V8_NATIVE_REGEXP

ExternalReference ExternalReference::re_check_stack_guard_state() {
  Address function;
#ifdef V8_TARGET_ARCH_X64
  function = FUNCTION_ADDR(RegExpMacroAssemblerX64::CheckStackGuardState);
#elif V8_TARGET_ARCH_IA32
  function = FUNCTION_ADDR(RegExpMacroAssemblerIA32::CheckStackGuardState);
#elif V8_TARGET_ARCH_ARM
  function = FUNCTION_ADDR(RegExpMacroAssemblerARM::CheckStackGuardState);
#else
  UNREACHABLE("Unexpected architecture");
#endif
  return ExternalReference(Redirect(function));
}

ExternalReference ExternalReference::re_grow_stack() {
  return ExternalReference(
      Redirect(FUNCTION_ADDR(NativeRegExpMacroAssembler::GrowStack)));
}

ExternalReference ExternalReference::re_case_insensitive_compare_uc16() {
  return ExternalReference(Redirect(
      FUNCTION_ADDR(NativeRegExpMacroAssembler::CaseInsensitiveCompareUC16)));
}

#endif


static double add_two_doubles(double x, double y) {
  return x + y;
}


static double sub_two_doubles(double x, double y) {
  return x - y;
}


static double mul_two_doubles(double x, double y) {
  return x * y;
}


static double div_two_doubles(double x, double y) {
  return x / y;
}


static double mod_two_doubles(double x, double y) {
  return fmod(x, y);
}


static int native_compare_doubles(double x, double y) {
  if (x == y) return 0;
  return x < y ? 1 : -1;
}


ExternalReference ExternalReference::double_fp_operation(
    Token::Value operation) {
  typedef double BinaryFPOperation(double x, double y);
  BinaryFPOperation* function = NULL;
  switch (operation) {
    case Token::ADD:
      function = &add_two_doubles;
      break;
    case Token::SUB:
      function = &sub_two_doubles;
      break;
    case Token::MUL:
      function = &mul_two_doubles;
      break;
    case Token::DIV:
      function = &div_two_doubles;
      break;
    case Token::MOD:
      function = &mod_two_doubles;
      break;
    default:
      UNREACHABLE();
  }
  // Passing true as 2nd parameter indicates that they return an fp value.
  return ExternalReference(Redirect(FUNCTION_ADDR(function), true));
}


ExternalReference ExternalReference::compare_doubles() {
  return ExternalReference(Redirect(FUNCTION_ADDR(native_compare_doubles),
                                    false));
}


ExternalReferenceRedirector* ExternalReference::redirector_ = NULL;


#ifdef ENABLE_DEBUGGER_SUPPORT
ExternalReference ExternalReference::debug_break() {
  return ExternalReference(Redirect(FUNCTION_ADDR(Debug::Break)));
}


ExternalReference ExternalReference::debug_step_in_fp_address() {
  return ExternalReference(Debug::step_in_fp_addr());
}
#endif

} }  // namespace v8::internal
