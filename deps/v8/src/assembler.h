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

namespace v8 {

// Forward declarations.
class ApiFunction;

namespace internal {

// Forward declarations.
class InstructionStream;
class Isolate;
class SCTableReference;
class SourcePosition;
class StatsCounter;

// -----------------------------------------------------------------------------
// Optimization for far-jmp like instructions that can be replaced by shorter.

class JumpOptimizationInfo {
 public:
  bool is_collecting() const { return stage_ == kCollection; }
  bool is_optimizing() const { return stage_ == kOptimization; }
  void set_optimizing() { stage_ = kOptimization; }

  bool is_optimizable() const { return optimizable_; }
  void set_optimizable() { optimizable_ = true; }

  std::vector<uint32_t>& farjmp_bitmap() { return farjmp_bitmap_; }

 private:
  enum { kCollection, kOptimization } stage_ = kCollection;
  bool optimizable_ = false;
  std::vector<uint32_t> farjmp_bitmap_;
};

// -----------------------------------------------------------------------------
// Platform independent assembler base class.

enum class CodeObjectRequired { kNo, kYes };


class AssemblerBase: public Malloced {
 public:
  struct IsolateData {
    explicit IsolateData(Isolate* isolate);
    IsolateData(const IsolateData&) = default;

    bool serializer_enabled_;
#if V8_TARGET_ARCH_X64
    Address code_range_start_;
#endif
  };

  AssemblerBase(IsolateData isolate_data, void* buffer, int buffer_size);
  virtual ~AssemblerBase();

  IsolateData isolate_data() const { return isolate_data_; }

  bool serializer_enabled() const { return isolate_data_.serializer_enabled_; }
  void enable_serializer() { isolate_data_.serializer_enabled_ = true; }

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

 protected:
  // The buffer into which code and relocation info are generated. It could
  // either be owned by the assembler or be provided externally.
  byte* buffer_;
  int buffer_size_;
  bool own_buffer_;

  void set_constant_pool_available(bool available) {
    if (FLAG_enable_embedded_constant_pool) {
      constant_pool_available_ = available;
    } else {
      // Embedded constant pool not supported on this architecture.
      UNREACHABLE();
    }
  }

  // The program counter, which points into the buffer above and moves forward.
  byte* pc_;

 private:
  IsolateData isolate_data_;
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
class DontEmitDebugCodeScope BASE_EMBEDDED {
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
class CpuFeatureScope BASE_EMBEDDED {
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
  // Define a destructor to avoid unused variable warnings.
  ~CpuFeatureScope() {}
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


enum SaveFPRegsMode { kDontSaveFPRegs, kSaveFPRegs };

enum ArgvMode { kArgvOnStack, kArgvInRegister };

// Specifies whether to perform icache flush operations on RelocInfo updates.
// If FLUSH_ICACHE_IF_NEEDED, the icache will always be flushed if an
// instruction was modified. If SKIP_ICACHE_FLUSH the flush will always be
// skipped (only use this if you will flush the icache manually before it is
// executed).
enum ICacheFlushMode { FLUSH_ICACHE_IF_NEEDED, SKIP_ICACHE_FLUSH };

// -----------------------------------------------------------------------------
// Relocation information


// Relocation information consists of the address (pc) of the datum
// to which the relocation information applies, the relocation mode
// (rmode), and an optional data field. The relocation mode may be
// "descriptive" and not indicate a need for relocation, but simply
// describe a property of the datum. Such rmodes are useful for GC
// and nice disassembly output.

class RelocInfo {
 public:
  enum Flag : uint8_t {
    kNoFlags = 0,
    kInNativeWasmCode = 1u << 0,  // Reloc info belongs to native wasm code.
  };
  typedef base::Flags<Flag> Flags;

  // This string is used to add padding comments to the reloc info in cases
  // where we are not sure to have enough space for patching in during
  // lazy deoptimization. This is the case if we have indirect calls for which
  // we do not normally record relocation info.
  static const char* const kFillerCommentString;

  // The minimum size of a comment is equal to two bytes for the extra tagged
  // pc and kPointerSize for the actual pointer to the comment.
  static const int kMinRelocCommentSize = 2 + kPointerSize;

  // The maximum size for a call instruction including pc-jump.
  static const int kMaxCallSize = 6;

  // The maximum pc delta that will use the short encoding.
  static const int kMaxSmallPCDelta;

  enum Mode : int8_t {
    // Please note the order is important (see IsCodeTarget, IsGCRelocMode).
    CODE_TARGET,
    EMBEDDED_OBJECT,
    WASM_GLOBAL_HANDLE,
    WASM_CALL,
    JS_TO_WASM_CALL,

    RUNTIME_ENTRY,
    COMMENT,

    EXTERNAL_REFERENCE,  // The address of an external C++ function.
    INTERNAL_REFERENCE,  // An address inside the same function.

    // Encoded internal reference, used only on MIPS, MIPS64 and PPC.
    INTERNAL_REFERENCE_ENCODED,

    // An off-heap instruction stream target. See http://goo.gl/Z2HUiM.
    OFF_HEAP_TARGET,

    // Marks constant and veneer pools. Only used on ARM and ARM64.
    // They use a custom noncompact encoding.
    CONST_POOL,
    VENEER_POOL,

    DEOPT_SCRIPT_OFFSET,
    DEOPT_INLINING_ID,  // Deoptimization source position.
    DEOPT_REASON,       // Deoptimization reason index.
    DEOPT_ID,           // Deoptimization inlining id.

    // This is not an actual reloc mode, but used to encode a long pc jump that
    // cannot be encoded as part of another record.
    PC_JUMP,

    // Points to a wasm code table entry.
    WASM_CODE_TABLE_ENTRY,

    // Pseudo-types
    NUMBER_OF_MODES,
    NONE,  // never recorded value

    FIRST_REAL_RELOC_MODE = CODE_TARGET,
    LAST_REAL_RELOC_MODE = VENEER_POOL,
    LAST_CODE_ENUM = CODE_TARGET,
    LAST_GCED_ENUM = EMBEDDED_OBJECT,
    FIRST_SHAREABLE_RELOC_MODE = RUNTIME_ENTRY,
  };

  STATIC_ASSERT(NUMBER_OF_MODES <= kBitsPerInt);

  RelocInfo() = default;

  RelocInfo(byte* pc, Mode rmode, intptr_t data, Code* host)
      : pc_(pc), rmode_(rmode), data_(data), host_(host) {}

  static inline bool IsRealRelocMode(Mode mode) {
    return mode >= FIRST_REAL_RELOC_MODE && mode <= LAST_REAL_RELOC_MODE;
  }
  static inline bool IsCodeTarget(Mode mode) {
    return mode <= LAST_CODE_ENUM;
  }
  static inline bool IsEmbeddedObject(Mode mode) {
    return mode == EMBEDDED_OBJECT;
  }
  static inline bool IsRuntimeEntry(Mode mode) {
    return mode == RUNTIME_ENTRY;
  }
  static inline bool IsWasmCall(Mode mode) { return mode == WASM_CALL; }
  // Is the relocation mode affected by GC?
  static inline bool IsGCRelocMode(Mode mode) {
    return mode <= LAST_GCED_ENUM;
  }
  static inline bool IsComment(Mode mode) {
    return mode == COMMENT;
  }
  static inline bool IsConstPool(Mode mode) {
    return mode == CONST_POOL;
  }
  static inline bool IsVeneerPool(Mode mode) {
    return mode == VENEER_POOL;
  }
  static inline bool IsDeoptPosition(Mode mode) {
    return mode == DEOPT_SCRIPT_OFFSET || mode == DEOPT_INLINING_ID;
  }
  static inline bool IsDeoptReason(Mode mode) {
    return mode == DEOPT_REASON;
  }
  static inline bool IsDeoptId(Mode mode) {
    return mode == DEOPT_ID;
  }
  static inline bool IsExternalReference(Mode mode) {
    return mode == EXTERNAL_REFERENCE;
  }
  static inline bool IsInternalReference(Mode mode) {
    return mode == INTERNAL_REFERENCE;
  }
  static inline bool IsInternalReferenceEncoded(Mode mode) {
    return mode == INTERNAL_REFERENCE_ENCODED;
  }
  static inline bool IsOffHeapTarget(Mode mode) {
    return mode == OFF_HEAP_TARGET;
  }
  static inline bool IsNone(Mode mode) { return mode == NONE; }
  static inline bool IsWasmReference(Mode mode) {
    return IsWasmPtrReference(mode);
  }
  static inline bool IsWasmPtrReference(Mode mode) {
    return mode == WASM_GLOBAL_HANDLE || mode == WASM_CALL ||
           mode == JS_TO_WASM_CALL;
  }

  static constexpr int ModeMask(Mode mode) { return 1 << mode; }

  // Accessors
  byte* pc() const { return pc_; }
  void set_pc(byte* pc) { pc_ = pc; }
  Mode rmode() const {  return rmode_; }
  intptr_t data() const { return data_; }
  Code* host() const { return host_; }
  Address constant_pool() const { return constant_pool_; }
  void set_constant_pool(Address constant_pool) {
    constant_pool_ = constant_pool;
  }

  // Apply a relocation by delta bytes. When the code object is moved, PC
  // relative addresses have to be updated as well as absolute addresses
  // inside the code (internal references).
  // Do not forget to flush the icache afterwards!
  INLINE(void apply(intptr_t delta));

  // Is the pointer this relocation info refers to coded like a plain pointer
  // or is it strange in some way (e.g. relative or patched into a series of
  // instructions).
  bool IsCodedSpecially();

  // The static pendant to IsCodedSpecially, just for off-heap targets. Used
  // during deserialization, when we don't actually have a RelocInfo handy.
  static bool OffHeapTargetIsCodedSpecially();

  // If true, the pointer this relocation info refers to is an entry in the
  // constant pool, otherwise the pointer is embedded in the instruction stream.
  bool IsInConstantPool();

  Address global_handle() const;
  Address js_to_wasm_address() const;
  Address wasm_call_address() const;

  void set_target_address(
      Address target,
      WriteBarrierMode write_barrier_mode = UPDATE_WRITE_BARRIER,
      ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED);

  void set_global_handle(Address address, ICacheFlushMode icache_flush_mode =
                                              FLUSH_ICACHE_IF_NEEDED);
  void set_wasm_call_address(
      Address, ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED);
  void set_js_to_wasm_address(
      Address, ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED);

  // this relocation applies to;
  // can only be called if IsCodeTarget(rmode_) || IsRuntimeEntry(rmode_)
  INLINE(Address target_address());
  INLINE(HeapObject* target_object());
  INLINE(Handle<HeapObject> target_object_handle(Assembler* origin));
  INLINE(void set_target_object(
      HeapObject* target,
      WriteBarrierMode write_barrier_mode = UPDATE_WRITE_BARRIER,
      ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED));
  INLINE(Address target_runtime_entry(Assembler* origin));
  INLINE(void set_target_runtime_entry(
      Address target,
      WriteBarrierMode write_barrier_mode = UPDATE_WRITE_BARRIER,
      ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED));
  INLINE(Address target_off_heap_target());
  INLINE(Cell* target_cell());
  INLINE(Handle<Cell> target_cell_handle());
  INLINE(void set_target_cell(
      Cell* cell, WriteBarrierMode write_barrier_mode = UPDATE_WRITE_BARRIER,
      ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED));
  INLINE(void set_wasm_code_table_entry(
      Address, ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED));
  INLINE(void set_target_external_reference(
      Address, ICacheFlushMode icache_flush_mode = FLUSH_ICACHE_IF_NEEDED));

  // Returns the address of the constant pool entry where the target address
  // is held.  This should only be called if IsInConstantPool returns true.
  INLINE(Address constant_pool_entry_address());

  // Read the address of the word containing the target_address in an
  // instruction stream.  What this means exactly is architecture-independent.
  // The only architecture-independent user of this function is the serializer.
  // The serializer uses it to find out how many raw bytes of instruction to
  // output before the next target.  Architecture-independent code shouldn't
  // dereference the pointer it gets back from this.
  INLINE(Address target_address_address());

  // This indicates how much space a target takes up when deserializing a code
  // stream.  For most architectures this is just the size of a pointer.  For
  // an instruction like movw/movt where the target bits are mixed into the
  // instruction bits the size of the target will be zero, indicating that the
  // serializer should not step forwards in memory after a target is resolved
  // and written.  In this case the target_address_address function above
  // should return the end of the instructions to be patched, allowing the
  // deserializer to deserialize the instructions as raw bytes and put them in
  // place, ready to be patched with the target.
  INLINE(int target_address_size());

  // Read the reference in the instruction this relocation
  // applies to; can only be called if rmode_ is EXTERNAL_REFERENCE.
  INLINE(Address target_external_reference());

  // Read the reference in the instruction this relocation
  // applies to; can only be called if rmode_ is INTERNAL_REFERENCE.
  INLINE(Address target_internal_reference());

  // Return the reference address this relocation applies to;
  // can only be called if rmode_ is INTERNAL_REFERENCE.
  INLINE(Address target_internal_reference_address());

  // Wipe out a relocation to a fixed value, used for making snapshots
  // reproducible.
  INLINE(void WipeOut());

  template <typename ObjectVisitor>
  inline void Visit(ObjectVisitor* v);

#ifdef DEBUG
  // Check whether the given code contains relocation information that
  // either is position-relative or movable by the garbage collector.
  static bool RequiresRelocation(const CodeDesc& desc);
#endif

#ifdef ENABLE_DISASSEMBLER
  // Printing
  static const char* RelocModeName(Mode rmode);
  void Print(Isolate* isolate, std::ostream& os);  // NOLINT
#endif  // ENABLE_DISASSEMBLER
#ifdef VERIFY_HEAP
  void Verify(Isolate* isolate);
#endif

  static const int kCodeTargetMask = (1 << (LAST_CODE_ENUM + 1)) - 1;
  static const int kApplyMask;  // Modes affected by apply.  Depends on arch.

 private:
  void set_embedded_address(Address address, ICacheFlushMode flush_mode);
  void set_embedded_size(uint32_t size, ICacheFlushMode flush_mode);

  uint32_t embedded_size() const;
  Address embedded_address() const;

  // On ARM, note that pc_ is the address of the constant pool entry
  // to be relocated and not the address of the instruction
  // referencing the constant pool entry (except when rmode_ ==
  // comment).
  byte* pc_;
  Mode rmode_;
  intptr_t data_ = 0;
  Code* host_;
  Address constant_pool_ = nullptr;
  Flags flags_;
  friend class RelocIterator;
};


// RelocInfoWriter serializes a stream of relocation info. It writes towards
// lower addresses.
class RelocInfoWriter BASE_EMBEDDED {
 public:
  RelocInfoWriter() : pos_(nullptr), last_pc_(nullptr) {}

  byte* pos() const { return pos_; }
  byte* last_pc() const { return last_pc_; }

  void Write(const RelocInfo* rinfo);

  // Update the state of the stream after reloc info buffer
  // and/or code is moved while the stream is active.
  void Reposition(byte* pos, byte* pc) {
    pos_ = pos;
    last_pc_ = pc;
  }

  // Max size (bytes) of a written RelocInfo. Longest encoding is
  // ExtraTag, VariableLengthPCJump, ExtraTag, pc_delta, data_delta.
  static constexpr int kMaxSize = 1 + 4 + 1 + 1 + kPointerSize;

 private:
  inline uint32_t WriteLongPCJump(uint32_t pc_delta);

  inline void WriteShortTaggedPC(uint32_t pc_delta, int tag);
  inline void WriteShortData(intptr_t data_delta);

  inline void WriteMode(RelocInfo::Mode rmode);
  inline void WriteModeAndPC(uint32_t pc_delta, RelocInfo::Mode rmode);
  inline void WriteIntData(int data_delta);
  inline void WriteData(intptr_t data_delta);

  byte* pos_;
  byte* last_pc_;

  DISALLOW_COPY_AND_ASSIGN(RelocInfoWriter);
};


// A RelocIterator iterates over relocation information.
// Typical use:
//
//   for (RelocIterator it(code); !it.done(); it.next()) {
//     // do something with it.rinfo() here
//   }
//
// A mask can be specified to skip unwanted modes.
class RelocIterator: public Malloced {
 public:
  // Create a new iterator positioned at
  // the beginning of the reloc info.
  // Relocation information with mode k is included in the
  // iteration iff bit k of mode_mask is set.
  explicit RelocIterator(Code* code, int mode_mask = -1);
  explicit RelocIterator(const CodeDesc& desc, int mode_mask = -1);
  explicit RelocIterator(Vector<byte> instructions,
                         Vector<const byte> reloc_info, Address const_pool,
                         int mode_mask = -1);
  RelocIterator(RelocIterator&&) = default;
  RelocIterator& operator=(RelocIterator&&) = default;

  // Iteration
  bool done() const { return done_; }
  void next();

  // Return pointer valid until next next().
  RelocInfo* rinfo() {
    DCHECK(!done());
    return &rinfo_;
  }

 private:
  // Advance* moves the position before/after reading.
  // *Read* reads from current byte(s) into rinfo_.
  // *Get* just reads and returns info on current byte.
  void Advance(int bytes = 1) { pos_ -= bytes; }
  int AdvanceGetTag();
  RelocInfo::Mode GetMode();

  void AdvanceReadLongPCJump();

  void ReadShortTaggedPC();
  void ReadShortData();

  void AdvanceReadPC();
  void AdvanceReadInt();
  void AdvanceReadData();

  // If the given mode is wanted, set it in rinfo_ and return true.
  // Else return false. Used for efficiently skipping unwanted modes.
  bool SetMode(RelocInfo::Mode mode) {
    return (mode_mask_ & (1 << mode)) ? (rinfo_.rmode_ = mode, true) : false;
  }

  const byte* pos_;
  const byte* end_;
  RelocInfo rinfo_;
  bool done_ = false;
  const int mode_mask_;

  DISALLOW_COPY_AND_ASSIGN(RelocIterator);
};

// -----------------------------------------------------------------------------
// Utility functions

// Computes pow(x, y) with the special cases in the spec for Math.pow.
double power_helper(Isolate* isolate, double x, double y);
double power_double_int(double x, int y);
double power_double_double(double x, double y);


// -----------------------------------------------------------------------------
// Constant pool support

class ConstantPoolEntry {
 public:
  ConstantPoolEntry() {}
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
  int merged_index(void) const {
    DCHECK(is_merged());
    return merged_index_;
  }
  void set_merged_index(int index) {
    DCHECK(sharing_ok());
    merged_index_ = index;
    DCHECK(is_merged());
  }
  int offset(void) const {
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

class ConstantPoolBuilder BASE_EMBEDDED {
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

class HeapObjectRequest {
 public:
  explicit HeapObjectRequest(double heap_number, int offset = -1);
  explicit HeapObjectRequest(CodeStub* code_stub, int offset = -1);

  enum Kind { kHeapNumber, kCodeStub };
  Kind kind() const { return kind_; }

  double heap_number() const {
    DCHECK_EQ(kind(), kHeapNumber);
    return value_.heap_number;
  }

  CodeStub* code_stub() const {
    DCHECK_EQ(kind(), kCodeStub);
    return value_.code_stub;
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
  } value_;

  int offset_;
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
  static constexpr int bit() {
    return 1 << code<reg_code>();
  }

  static SubType from_code(int code) {
    DCHECK_LE(0, code);
    DCHECK_GT(kNumRegisters, code);
    return SubType{code};
  }

  template <RegisterCode... reg_codes>
  static constexpr RegList ListOf() {
    return CombineRegLists(RegisterBase::bit<reg_codes>()...);
  }

  bool is_valid() const { return reg_code_ != kCode_no_reg; }

  int code() const {
    DCHECK(is_valid());
    return reg_code_;
  }

  int bit() const { return 1 << code(); }

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
