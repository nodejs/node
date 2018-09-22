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
#include "src/snapshot/snapshot.h"
#include "src/string-constants.h"

namespace v8 {
namespace internal {

AssemblerOptions AssemblerOptions::EnableV8AgnosticCode() const {
  AssemblerOptions options = *this;
  options.v8_agnostic_code = true;
  options.record_reloc_info_for_serialization = false;
  options.enable_root_array_delta_access = false;
  // Inherit |enable_simulator_code| value.
  options.isolate_independent_code = false;
  options.inline_offheap_trampolines = false;
  // Inherit |code_range_start| value.
  // Inherit |use_pc_relative_calls_and_jumps| value.
  return options;
}

AssemblerOptions AssemblerOptions::Default(
    Isolate* isolate, bool explicitly_support_serialization) {
  AssemblerOptions options;
  bool serializer =
      isolate->serializer_enabled() || explicitly_support_serialization;
  options.record_reloc_info_for_serialization = serializer;
  options.enable_root_array_delta_access = !serializer;
#ifdef USE_SIMULATOR
  // Don't generate simulator specific code if we are building a snapshot, which
  // might be run on real hardware.
  options.enable_simulator_code = !serializer;
#endif
  options.inline_offheap_trampolines = !serializer;

#if V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_ARM64
  const base::AddressRegion& code_range =
      isolate->heap()->memory_allocator()->code_range();
  DCHECK_IMPLIES(code_range.begin() != kNullAddress, !code_range.is_empty());
  options.code_range_start = code_range.begin();
#endif
  return options;
}

// -----------------------------------------------------------------------------
// Implementation of AssemblerBase

AssemblerBase::AssemblerBase(const AssemblerOptions& options, void* buffer,
                             int buffer_size)
    : options_(options),
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
  StdoutStream os;
  v8::internal::Disassembler::Decode(isolate, &os, buffer_, pc_);
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

HeapObjectRequest::HeapObjectRequest(const StringConstantBase* string,
                                     int offset)
    : kind_(kStringConstant), offset_(offset) {
  value_.string = string;
  DCHECK_NOT_NULL(value_.string);
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

void AssemblerBase::RequestHeapObject(HeapObjectRequest request) {
  DCHECK(!options().v8_agnostic_code);
  request.set_offset(pc_offset());
  heap_object_requests_.push_front(request);
}

int AssemblerBase::AddCodeTarget(Handle<Code> target) {
  DCHECK(!options().v8_agnostic_code);
  int current = static_cast<int>(code_targets_.size());
  if (current > 0 && !target.is_null() &&
      code_targets_.back().address() == target.address()) {
    // Optimization if we keep jumping to the same code target.
    return current - 1;
  } else {
    code_targets_.push_back(target);
    return current;
  }
}

Handle<Code> AssemblerBase::GetCodeTarget(intptr_t code_target_index) const {
  DCHECK(!options().v8_agnostic_code);
  DCHECK_LE(0, code_target_index);
  DCHECK_LT(code_target_index, code_targets_.size());
  return code_targets_[code_target_index];
}

void AssemblerBase::UpdateCodeTarget(intptr_t code_target_index,
                                     Handle<Code> code) {
  DCHECK(!options().v8_agnostic_code);
  DCHECK_LE(0, code_target_index);
  DCHECK_LT(code_target_index, code_targets_.size());
  code_targets_[code_target_index] = code;
}

}  // namespace internal
}  // namespace v8
