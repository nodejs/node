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

#ifndef V8_ASSEMBLER_H_
#define V8_ASSEMBLER_H_

#include <forward_list>
#include <iosfwd>
#include <map>

#include "src/allocation.h"
#include "src/code-reference.h"
#include "src/contexts.h"
#include "src/deoptimize-reason.h"
#include "src/double.h"
#include "src/external-reference.h"
#include "src/flags.h"
#include "src/globals.h"
#include "src/label.h"
#include "src/objects.h"
#include "src/register-configuration.h"
#include "src/reglist.h"
#include "src/reloc-info.h"

namespace v8 {

// Forward declarations.
class ApiFunction;

namespace internal {

// Forward declarations.
class EmbeddedData;
class InstructionStream;
class Isolate;
class SCTableReference;
class SourcePosition;
class StatsCounter;
class StringConstantBase;

// -----------------------------------------------------------------------------
// Optimization for far-jmp like instructions that can be replaced by shorter.

class JumpOptimizationInfo {
 public:
  bool is_collecting() const { return stage_ == kCollection; }
  bool is_optimizing() const { return stage_ == kOptimization; }
  void set_optimizing() { stage_ = kOptimization; }

  bool is_optimizable() const { return optimizable_; }
  void set_optimizable() { optimizable_ = true; }

  // Used to verify the instruction sequence is always the same in two stages.
  size_t hash_code() const { return hash_code_; }
  void set_hash_code(size_t hash_code) { hash_code_ = hash_code; }

  std::vector<uint32_t>& farjmp_bitmap() { return farjmp_bitmap_; }

 private:
  enum { kCollection, kOptimization } stage_ = kCollection;
  bool optimizable_ = false;
  std::vector<uint32_t> farjmp_bitmap_;
  size_t hash_code_ = 0u;
};

class HeapObjectRequest {
 public:
  explicit HeapObjectRequest(double heap_number, int offset = -1);
  explicit HeapObjectRequest(CodeStub* code_stub, int offset = -1);
  explicit HeapObjectRequest(const StringConstantBase* string, int offset = -1);

  enum Kind { kHeapNumber, kCodeStub, kStringConstant };
  Kind kind() const { return kind_; }

  double heap_number() const {
    DCHECK_EQ(kind(), kHeapNumber);
    return value_.heap_number;
  }

  CodeStub* code_stub() const {
    DCHECK_EQ(kind(), kCodeStub);
    return value_.code_stub;
  }

  const StringConstantBase* string() const {
    DCHECK_EQ(kind(), kStringConstant);
    return value_.string;
  }

  // The code buffer offset at the time of the request.
  int offset() const {
    DCHECK_GE(offset_, 0);
    return offset_;
  }
  void set_offset(int offset) {
    DCHECK_LT(offset_, 0);
    offset_ = offset;
    DCHECK_GE(offset_, 0);
  }

 private:
  Kind kind_;

  union {
    double heap_number;
    CodeStub* code_stub;
    const StringConstantBase* string;
  } value_;

  int offset_;
};

// -----------------------------------------------------------------------------
// Platform independent assembler base class.

enum class CodeObjectRequired { kNo, kYes };

struct V8_EXPORT_PRIVATE AssemblerOptions {
  // Prohibits using any V8-specific features of assembler like (isolates,
  // heap objects, external references, etc.).
  bool v8_agnostic_code = false;
  // Recording reloc info for external references and off-heap targets is
  // needed whenever code is serialized, e.g. into the snapshot or as a WASM
  // module. This flag allows this reloc info to be disabled for code that
  // will not survive process destruction.
  bool record_reloc_info_for_serialization = true;
  // Recording reloc info can be disabled wholesale. This is needed when the
  // assembler is used on existing code directly (e.g. JumpTableAssembler)
  // without any buffer to hold reloc information.
  bool disable_reloc_info_for_patching = false;
  // Enables access to exrefs by computing a delta from the root array.
  // Only valid if code will not survive the process.
  bool enable_root_array_delta_access = false;
  // Enables specific assembler sequences only used for the simulator.
  bool enable_simulator_code = false;
  // Enables use of isolate-independent constants, indirected through the
  // root array.
  // (macro assembler feature).
  bool isolate_independent_code = false;
  // Enables the use of isolate-independent builtins through an off-heap
  // trampoline. (macro assembler feature).
  bool inline_offheap_trampolines = false;
  // On some platforms, all code is within a given range in the process,
  // and the start of this range is configured here.
  Address code_range_start = 0;
  // Enable pc-relative calls/jumps on platforms that support it. When setting
  // this flag, the code range must be small enough to fit all offsets into
  // the instruction immediates.
  bool use_pc_relative_calls_and_jumps = false;

  // Constructs V8-agnostic set of options from current state.
  AssemblerOptions EnableV8AgnosticCode() const;

  static AssemblerOptions Default(
      Isolate* isolate, bool explicitly_support_serialization = false);
};

class V8_EXPORT_PRIVATE AssemblerBase : public Malloced {
 public:
  AssemblerBase(const AssemblerOptions& options, void* buffer, int buffer_size);
  virtual ~AssemblerBase();

  const AssemblerOptions& options() const { return options_; }

  bool emit_debug_code() const { return emit_debug_code_; }
  void set_emit_debug_code(bool value) { emit_debug_code_ = value; }

  bool predictable_code_size() const { return predictable_code_size_; }
  void set_predictable_code_size(bool value) { predictable_code_size_ = value; }

  uint64_t enabled_cpu_features() const { return enabled_cpu_features_; }
  void set_enabled_cpu_features(uint64_t features) {
    enabled_cpu_features_ = features;
  }
  // Features are usually enabled by CpuFeatureScope, which also asserts that
  // the features are supported before they are enabled.
  bool IsEnabled(CpuFeature f) {
    return (enabled_cpu_features_ & (static_cast<uint64_t>(1) << f)) != 0;
  }
  void EnableCpuFeature(CpuFeature f) {
    enabled_cpu_features_ |= (static_cast<uint64_t>(1) << f);
  }

  bool is_constant_pool_available() const {
    if (FLAG_enable_embedded_constant_pool) {
      return constant_pool_available_;
    } else {
      // Embedded constant pool not supported on this architecture.
      UNREACHABLE();
    }
  }

  JumpOptimizationInfo* jump_optimization_info() {
    return jump_optimization_info_;
  }
  void set_jump_optimization_info(JumpOptimizationInfo* jump_opt) {
    jump_optimization_info_ = jump_opt;
  }

  // Overwrite a host NaN with a quiet target NaN.  Used by mksnapshot for
  // cross-snapshotting.
  static void QuietNaN(HeapObject* nan) { }

  int pc_offset() const { return static_cast<int>(pc_ - buffer_); }

  // This function is called when code generation is aborted, so that
  // the assembler could clean up internal data structures.
  virtual void AbortedCodeGeneration() { }

  // Debugging
  void Print(Isolate* isolate);

  static const int kMinimalBufferSize = 4*KB;

  static void FlushICache(void* start, size_t size);
  static void FlushICache(Address start, size_t size) {
    return FlushICache(reinterpret_cast<void*>(start), size);
  }

  // Used to print the name of some special registers.
  static const char* GetSpecialRegisterName(int code) { return "UNKNOWN"; }

 protected:
  // Add 'target' to the {code_targets_} vector, if necessary, and return the
  // offset at which it is stored.
  int AddCodeTarget(Handle<Code> target);
  Handle<Code> GetCodeTarget(intptr_t code_target_index) const;
  // Update to the code target at {code_target_index} to {target}.
  void UpdateCodeTarget(intptr_t code_target_index, Handle<Code> target);
  // Reserves space in the code target vector.
  void ReserveCodeTargetSpace(size_t num_of_code_targets) {
    code_targets_.reserve(num_of_code_targets);
  }

  // The buffer into which code and relocation info are generated. It could
  // either be owned by the assembler or be provided externally.
  byte* buffer_;
  int buffer_size_;
  bool own_buffer_;
  std::forward_list<HeapObjectRequest> heap_object_requests_;
  // The program counter, which points into the buffer above and moves forward.
  // TODO(jkummerow): This should probably have type {Address}.
  byte* pc_;

  void set_constant_pool_available(bool available) {
    if (FLAG_enable_embedded_constant_pool) {
      constant_pool_available_ = available;
    } else {
      // Embedded constant pool not supported on this architecture.
      UNREACHABLE();
    }
  }

  // {RequestHeapObject} records the need for a future heap number allocation,
  // code stub generation or string allocation. After code assembly, each
  // platform's {Assembler::AllocateAndInstallRequestedHeapObjects} will
  // allocate these objects and place them where they are expected (determined
  // by the pc offset associated with each request).
  void RequestHeapObject(HeapObjectRequest request);

  bool ShouldRecordRelocInfo(RelocInfo::Mode rmode) const {
    DCHECK(!RelocInfo::IsNone(rmode));
    if (options().disable_reloc_info_for_patching) return false;
    if (RelocInfo::IsOnlyForSerializer(rmode) &&
        !options().record_reloc_info_for_serialization && !emit_debug_code()) {
      return false;
    }
    return true;
  }

 private:
  // Before we copy code into the code space, we sometimes cannot encode
  // call/jump code targets as we normally would, as the difference between the
  // instruction's location in the temporary buffer and the call target is not
  // guaranteed to fit in the instruction's offset field. We keep track of the
  // code handles we encounter in calls in this vector, and encode the index of
  // the code handle in the vector instead.
  std::vector<Handle<Code>> code_targets_;

  const AssemblerOptions options_;
  uint64_t enabled_cpu_features_;
  bool emit_debug_code_;
  bool predictable_code_size_;

  // Indicates whether the constant pool can be accessed, which is only possible
  // if the pp register points to the current code object's constant pool.
  bool constant_pool_available_;

  JumpOptimizationInfo* jump_optimization_info_;

  // Constant pool.
  friend class FrameAndConstantPoolScope;
  friend class ConstantPoolUnavailableScope;
};

// Avoids emitting debug code during the lifetime of this scope object.
class DontEmitDebugCodeScope {
 public:
  explicit DontEmitDebugCodeScope(AssemblerBase* assembler)
      : assembler_(assembler), old_value_(assembler->emit_debug_code()) {
    assembler_->set_emit_debug_code(false);
  }
  ~DontEmitDebugCodeScope() {
    assembler_->set_emit_debug_code(old_value_);
  }
 private:
  AssemblerBase* assembler_;
  bool old_value_;
};


// Avoids using instructions that vary in size in unpredictable ways between the
// snapshot and the running VM.
class PredictableCodeSizeScope {
 public:
  PredictableCodeSizeScope(AssemblerBase* assembler, int expected_size);
  ~PredictableCodeSizeScope();

 private:
  AssemblerBase* const assembler_;
  int const expected_size_;
  int const start_offset_;
  bool const old_value_;
};


// Enable a specified feature within a scope.
class CpuFeatureScope {
 public:
  enum CheckPolicy {
    kCheckSupported,
    kDontCheckSupported,
  };

#ifdef DEBUG
  CpuFeatureScope(AssemblerBase* assembler, CpuFeature f,
                  CheckPolicy check = kCheckSupported);
  ~CpuFeatureScope();

 private:
  AssemblerBase* assembler_;
  uint64_t old_enabled_;
#else
  CpuFeatureScope(AssemblerBase* assembler, CpuFeature f,
                  CheckPolicy check = kCheckSupported) {}
  ~CpuFeatureScope() {  // NOLINT (modernize-use-equals-default)
    // Define a destructor to avoid unused variable warnings.
  }
#endif
};

// CpuFeatures keeps track of which features are supported by the target CPU.
// Supported features must be enabled by a CpuFeatureScope before use.
// Example:
//   if (assembler->IsSupported(SSE3)) {
//     CpuFeatureScope fscope(assembler, SSE3);
//     // Generate code containing SSE3 instructions.
//   } else {
//     // Generate alternative code.
//   }
class CpuFeatures : public AllStatic {
 public:
  static void Probe(bool cross_compile) {
    STATIC_ASSERT(NUMBER_OF_CPU_FEATURES <= kBitsPerInt);
    if (initialized_) return;
    initialized_ = true;
    ProbeImpl(cross_compile);
  }

  static unsigned SupportedFeatures() {
    Probe(false);
    return supported_;
  }

  static bool IsSupported(CpuFeature f) {
    return (supported_ & (1u << f)) != 0;
  }

  static inline bool SupportsOptimizer();

  static inline bool SupportsWasmSimd128();

  static inline unsigned icache_line_size() {
    DCHECK_NE(icache_line_size_, 0);
    return icache_line_size_;
  }

  static inline unsigned dcache_line_size() {
    DCHECK_NE(dcache_line_size_, 0);
    return dcache_line_size_;
  }

  static void PrintTarget();
  static void PrintFeatures();

 private:
  friend class ExternalReference;
  friend class AssemblerBase;
  // Flush instruction cache.
  static void FlushICache(void* start, size_t size);

  // Platform-dependent implementation.
  static void ProbeImpl(bool cross_compile);

  static unsigned supported_;
  static unsigned icache_line_size_;
  static unsigned dcache_line_size_;
  static bool initialized_;
  DISALLOW_COPY_AND_ASSIGN(CpuFeatures);
};

// -----------------------------------------------------------------------------
// Utility functions

// Computes pow(x, y) with the special cases in the spec for Math.pow.
double power_helper(double x, double y);
double power_double_int(double x, int y);
double power_double_double(double x, double y);


// -----------------------------------------------------------------------------
// Constant pool support

class ConstantPoolEntry {
 public:
  ConstantPoolEntry() = default;
  ConstantPoolEntry(int position, intptr_t value, bool sharing_ok,
                    RelocInfo::Mode rmode = RelocInfo::NONE)
      : position_(position),
        merged_index_(sharing_ok ? SHARING_ALLOWED : SHARING_PROHIBITED),
        value_(value),
        rmode_(rmode) {}
  ConstantPoolEntry(int position, Double value,
                    RelocInfo::Mode rmode = RelocInfo::NONE)
      : position_(position),
        merged_index_(SHARING_ALLOWED),
        value64_(value.AsUint64()),
        rmode_(rmode) {}

  int position() const { return position_; }
  bool sharing_ok() const { return merged_index_ != SHARING_PROHIBITED; }
  bool is_merged() const { return merged_index_ >= 0; }
  int merged_index() const {
    DCHECK(is_merged());
    return merged_index_;
  }
  void set_merged_index(int index) {
    DCHECK(sharing_ok());
    merged_index_ = index;
    DCHECK(is_merged());
  }
  int offset() const {
    DCHECK_GE(merged_index_, 0);
    return merged_index_;
  }
  void set_offset(int offset) {
    DCHECK_GE(offset, 0);
    merged_index_ = offset;
  }
  intptr_t value() const { return value_; }
  uint64_t value64() const { return value64_; }
  RelocInfo::Mode rmode() const { return rmode_; }

  enum Type { INTPTR, DOUBLE, NUMBER_OF_TYPES };

  static int size(Type type) {
    return (type == INTPTR) ? kPointerSize : kDoubleSize;
  }

  enum Access { REGULAR, OVERFLOWED };

 private:
  int position_;
  int merged_index_;
  union {
    intptr_t value_;
    uint64_t value64_;
  };
  // TODO(leszeks): The way we use this, it could probably be packed into
  // merged_index_ if size is a concern.
  RelocInfo::Mode rmode_;
  enum { SHARING_PROHIBITED = -2, SHARING_ALLOWED = -1 };
};


// -----------------------------------------------------------------------------
// Embedded constant pool support

class ConstantPoolBuilder {
 public:
  ConstantPoolBuilder(int ptr_reach_bits, int double_reach_bits);

  // Add pointer-sized constant to the embedded constant pool
  ConstantPoolEntry::Access AddEntry(int position, intptr_t value,
                                     bool sharing_ok) {
    ConstantPoolEntry entry(position, value, sharing_ok);
    return AddEntry(entry, ConstantPoolEntry::INTPTR);
  }

  // Add double constant to the embedded constant pool
  ConstantPoolEntry::Access AddEntry(int position, Double value) {
    ConstantPoolEntry entry(position, value);
    return AddEntry(entry, ConstantPoolEntry::DOUBLE);
  }

  // Add double constant to the embedded constant pool
  ConstantPoolEntry::Access AddEntry(int position, double value) {
    return AddEntry(position, Double(value));
  }

  // Previews the access type required for the next new entry to be added.
  ConstantPoolEntry::Access NextAccess(ConstantPoolEntry::Type type) const;

  bool IsEmpty() {
    return info_[ConstantPoolEntry::INTPTR].entries.empty() &&
           info_[ConstantPoolEntry::INTPTR].shared_entries.empty() &&
           info_[ConstantPoolEntry::DOUBLE].entries.empty() &&
           info_[ConstantPoolEntry::DOUBLE].shared_entries.empty();
  }

  // Emit the constant pool.  Invoke only after all entries have been
  // added and all instructions have been emitted.
  // Returns position of the emitted pool (zero implies no constant pool).
  int Emit(Assembler* assm);

  // Returns the label associated with the start of the constant pool.
  // Linking to this label in the function prologue may provide an
  // efficient means of constant pool pointer register initialization
  // on some architectures.
  inline Label* EmittedPosition() { return &emitted_label_; }

 private:
  ConstantPoolEntry::Access AddEntry(ConstantPoolEntry& entry,
                                     ConstantPoolEntry::Type type);
  void EmitSharedEntries(Assembler* assm, ConstantPoolEntry::Type type);
  void EmitGroup(Assembler* assm, ConstantPoolEntry::Access access,
                 ConstantPoolEntry::Type type);

  struct PerTypeEntryInfo {
    PerTypeEntryInfo() : regular_count(0), overflow_start(-1) {}
    bool overflow() const {
      return (overflow_start >= 0 &&
              overflow_start < static_cast<int>(entries.size()));
    }
    int regular_reach_bits;
    int regular_count;
    int overflow_start;
    std::vector<ConstantPoolEntry> entries;
    std::vector<ConstantPoolEntry> shared_entries;
  };

  Label emitted_label_;  // Records pc_offset of emitted pool
  PerTypeEntryInfo info_[ConstantPoolEntry::NUMBER_OF_TYPES];
};

// Base type for CPU Registers.
//
// 1) We would prefer to use an enum for registers, but enum values are
// assignment-compatible with int, which has caused code-generation bugs.
//
// 2) By not using an enum, we are possibly preventing the compiler from
// doing certain constant folds, which may significantly reduce the
// code generated for some assembly instructions (because they boil down
// to a few constants). If this is a problem, we could change the code
// such that we use an enum in optimized mode, and the class in debug
// mode. This way we get the compile-time error checking in debug mode
// and best performance in optimized code.
template <typename SubType, int kAfterLastRegister>
class RegisterBase {
  // Internal enum class; used for calling constexpr methods, where we need to
  // pass an integral type as template parameter.
  enum class RegisterCode : int { kFirst = 0, kAfterLast = kAfterLastRegister };

 public:
  static constexpr int kCode_no_reg = -1;
  static constexpr int kNumRegisters = kAfterLastRegister;

  static constexpr SubType no_reg() { return SubType{kCode_no_reg}; }

  template <int code>
  static constexpr SubType from_code() {
    static_assert(code >= 0 && code < kNumRegisters, "must be valid reg code");
    return SubType{code};
  }

  constexpr operator RegisterCode() const {
    return static_cast<RegisterCode>(reg_code_);
  }

  template <RegisterCode reg_code>
  static constexpr int code() {
    static_assert(
        reg_code >= RegisterCode::kFirst && reg_code < RegisterCode::kAfterLast,
        "must be valid reg");
    return static_cast<int>(reg_code);
  }

  template <RegisterCode reg_code>
  static constexpr RegList bit() {
    return RegList{1} << code<reg_code>();
  }

  static SubType from_code(int code) {
    DCHECK_LE(0, code);
    DCHECK_GT(kNumRegisters, code);
    return SubType{code};
  }

  // Constexpr version (pass registers as template parameters).
  template <RegisterCode... reg_codes>
  static constexpr RegList ListOf() {
    return CombineRegLists(RegisterBase::bit<reg_codes>()...);
  }

  // Non-constexpr version (pass registers as method parameters).
  template <typename... Register>
  static RegList ListOf(Register... regs) {
    return CombineRegLists(regs.bit()...);
  }

  bool is_valid() const { return reg_code_ != kCode_no_reg; }

  int code() const {
    DCHECK(is_valid());
    return reg_code_;
  }

  RegList bit() const { return RegList{1} << code(); }

  inline constexpr bool operator==(SubType other) const {
    return reg_code_ == other.reg_code_;
  }
  inline constexpr bool operator!=(SubType other) const {
    return reg_code_ != other.reg_code_;
  }

 protected:
  explicit constexpr RegisterBase(int code) : reg_code_(code) {}
  int reg_code_;
};

template <typename SubType, int kAfterLastRegister>
inline std::ostream& operator<<(std::ostream& os,
                                RegisterBase<SubType, kAfterLastRegister> reg) {
  return reg.is_valid() ? os << "r" << reg.code() : os << "<invalid reg>";
}

}  // namespace internal
}  // namespace v8
#endif  // V8_ASSEMBLER_H_
