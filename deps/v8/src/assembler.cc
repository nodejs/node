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
#include "src/deoptimizer.h"
#include "src/disassembler.h"
#include "src/heap/heap-inl.h"  // For MemoryAllocator. TODO(jkummerow): Drop.
#include "src/isolate.h"
#include "src/ostreams.h"
#include "src/snapshot/embedded-data.h"
#include "src/snapshot/serializer-common.h"
#include "src/snapshot/snapshot.h"
#include "src/string-constants.h"
#include "src/vector.h"

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
  const bool serializer =
      isolate->serializer_enabled() || explicitly_support_serialization;
  const bool generating_embedded_builtin =
      isolate->IsGeneratingEmbeddedBuiltins();
  options.record_reloc_info_for_serialization = serializer;
  options.enable_root_array_delta_access =
      !serializer && !generating_embedded_builtin;
#ifdef USE_SIMULATOR
  // Don't generate simulator specific code if we are building a snapshot, which
  // might be run on real hardware.
  options.enable_simulator_code = !serializer;
#endif
  options.inline_offheap_trampolines =
      FLAG_embedded_builtins && !serializer && !generating_embedded_builtin;
#if V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_ARM64
  const base::AddressRegion& code_range =
      isolate->heap()->memory_allocator()->code_range();
  DCHECK_IMPLIES(code_range.begin() != kNullAddress, !code_range.is_empty());
  options.code_range_start = code_range.begin();
#endif
  return options;
}

namespace {

class DefaultAssemblerBuffer : public AssemblerBuffer {
 public:
  explicit DefaultAssemblerBuffer(int size)
      : buffer_(OwnedVector<uint8_t>::New(size)) {
#ifdef DEBUG
    ZapCode(reinterpret_cast<Address>(buffer_.start()), size);
#endif
  }

  byte* start() const override { return buffer_.start(); }

  int size() const override { return static_cast<int>(buffer_.size()); }

  std::unique_ptr<AssemblerBuffer> Grow(int new_size) override {
    DCHECK_LT(size(), new_size);
    return base::make_unique<DefaultAssemblerBuffer>(new_size);
  }

 private:
  OwnedVector<uint8_t> buffer_;
};

class ExternalAssemblerBufferImpl : public AssemblerBuffer {
 public:
  ExternalAssemblerBufferImpl(byte* start, int size)
      : start_(start), size_(size) {}

  byte* start() const override { return start_; }

  int size() const override { return size_; }

  std::unique_ptr<AssemblerBuffer> Grow(int new_size) override {
    FATAL("Cannot grow external assembler buffer");
  }

 private:
  byte* const start_;
  const int size_;
};

}  // namespace

std::unique_ptr<AssemblerBuffer> ExternalAssemblerBuffer(void* start,
                                                         int size) {
  return base::make_unique<ExternalAssemblerBufferImpl>(
      reinterpret_cast<byte*>(start), size);
}

std::unique_ptr<AssemblerBuffer> NewAssemblerBuffer(int size) {
  return base::make_unique<DefaultAssemblerBuffer>(size);
}

// -----------------------------------------------------------------------------
// Implementation of AssemblerBase

AssemblerBase::AssemblerBase(const AssemblerOptions& options,
                             std::unique_ptr<AssemblerBuffer> buffer)
    : buffer_(std::move(buffer)),
      options_(options),
      enabled_cpu_features_(0),
      emit_debug_code_(FLAG_debug_code),
      predictable_code_size_(false),
      constant_pool_available_(false),
      jump_optimization_info_(nullptr) {
  if (!buffer_) buffer_ = NewAssemblerBuffer(kMinimalBufferSize);
  buffer_start_ = buffer_->start();
  pc_ = buffer_start_;
}

AssemblerBase::~AssemblerBase() = default;

void AssemblerBase::Print(Isolate* isolate) {
  StdoutStream os;
  v8::internal::Disassembler::Decode(isolate, &os, buffer_start_, pc_);
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

HeapObjectRequest::HeapObjectRequest(double heap_number, int offset)
    : kind_(kHeapNumber), offset_(offset) {
  value_.heap_number = heap_number;
  DCHECK(!IsSmiDouble(value_.heap_number));
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

void Assembler::DataAlign(int m) {
  DCHECK(m >= 2 && base::bits::IsPowerOfTwo(m));
  while ((pc_offset() & (m - 1)) != 0) {
    // Pad with 0xcc (= int3 on ia32 and x64); the primary motivation is that
    // the disassembler expects to find valid instructions, but this is also
    // nice from a security point of view.
    db(0xcc);
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

void AssemblerBase::ReserveCodeTargetSpace(size_t num_of_code_targets) {
  code_targets_.reserve(num_of_code_targets);
}

int Assembler::WriteCodeComments() {
  if (!FLAG_code_comments || code_comments_writer_.entry_count() == 0) return 0;
  int offset = pc_offset();
  code_comments_writer_.Emit(this);
  int size = pc_offset() - offset;
  DCHECK_EQ(size, code_comments_writer_.section_size());
  return size;
}

}  // namespace internal
}  // namespace v8
