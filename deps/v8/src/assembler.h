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

#include "v8.h"

#include "allocation.h"
#include "builtins.h"
#include "gdb-jit.h"
#include "isolate.h"
#include "runtime.h"
#include "token.h"

namespace v8 {

class ApiFunction;

namespace internal {

class StatsCounter;
// -----------------------------------------------------------------------------
// Platform independent assembler base class.

class AssemblerBase: public Malloced {
 public:
  AssemblerBase(Isolate* isolate, void* buffer, int buffer_size);
  virtual ~AssemblerBase();

  Isolate* isolate() const { return isolate_; }
  int jit_cookie() const { return jit_cookie_; }

  bool emit_debug_code() const { return emit_debug_code_; }
  void set_emit_debug_code(bool value) { emit_debug_code_ = value; }

  bool predictable_code_size() const { return predictable_code_size_; }
  void set_predictable_code_size(bool value) { predictable_code_size_ = value; }

  uint64_t enabled_cpu_features() const { return enabled_cpu_features_; }
  void set_enabled_cpu_features(uint64_t features) {
    enabled_cpu_features_ = features;
  }
  bool IsEnabled(CpuFeature f) {
    return (enabled_cpu_features_ & (static_cast<uint64_t>(1) << f)) != 0;
  }

  // Overwrite a host NaN with a quiet target NaN.  Used by mksnapshot for
  // cross-snapshotting.
  static void QuietNaN(HeapObject* nan) { }

  int pc_offset() const { return static_cast<int>(pc_ - buffer_); }

  static const int kMinimalBufferSize = 4*KB;

 protected:
  // The buffer into which code and relocation info are generated. It could
  // either be owned by the assembler or be provided externally.
  byte* buffer_;
  int buffer_size_;
  bool own_buffer_;

  // The program counter, which points into the buffer above and moves forward.
  byte* pc_;

 private:
  Isolate* isolate_;
  int jit_cookie_;
  uint64_t enabled_cpu_features_;
  bool emit_debug_code_;
  bool predictable_code_size_;
};


// Avoids using instructions that vary in size in unpredictable ways between the
// snapshot and the running VM.
class PredictableCodeSizeScope {
 public:
  PredictableCodeSizeScope(AssemblerBase* assembler, int expected_size);
  ~PredictableCodeSizeScope();

 private:
  AssemblerBase* assembler_;
  int expected_size_;
  int start_offset_;
  bool old_value_;
};


// Enable a specified feature within a scope.
class CpuFeatureScope BASE_EMBEDDED {
 public:
#ifdef DEBUG
  CpuFeatureScope(AssemblerBase* assembler, CpuFeature f);
  ~CpuFeatureScope();

 private:
  AssemblerBase* assembler_;
  uint64_t old_enabled_;
#else
  CpuFeatureScope(AssemblerBase* assembler, CpuFeature f) {}
#endif
};


// Enable a unsupported feature within a scope for cross-compiling for a
// different CPU.
class PlatformFeatureScope BASE_EMBEDDED {
 public:
  explicit PlatformFeatureScope(CpuFeature f);
  ~PlatformFeatureScope();

 private:
  uint64_t old_cross_compile_;
};


// -----------------------------------------------------------------------------
// Labels represent pc locations; they are typically jump or call targets.
// After declaration, a label can be freely used to denote known or (yet)
// unknown pc location. Assembler::bind() is used to bind a label to the
// current pc. A label can be bound only once.

class Label BASE_EMBEDDED {
 public:
  enum Distance {
    kNear, kFar
  };

  INLINE(Label()) {
    Unuse();
    UnuseNear();
  }

  INLINE(~Label()) {
    ASSERT(!is_linked());
    ASSERT(!is_near_linked());
  }

  INLINE(void Unuse()) { pos_ = 0; }
  INLINE(void UnuseNear()) { near_link_pos_ = 0; }

  INLINE(bool is_bound() const) { return pos_ <  0; }
  INLINE(bool is_unused() const) { return pos_ == 0 && near_link_pos_ == 0; }
  INLINE(bool is_linked() const) { return pos_ >  0; }
  INLINE(bool is_near_linked() const) { return near_link_pos_ > 0; }

  // Returns the position of bound or linked labels. Cannot be used
  // for unused labels.
  int pos() const;
  int near_link_pos() const { return near_link_pos_ - 1; }

 private:
  // pos_ encodes both the binding state (via its sign)
  // and the binding position (via its value) of a label.
  //
  // pos_ <  0  bound label, pos() returns the jump target position
  // pos_ == 0  unused label
  // pos_ >  0  linked label, pos() returns the last reference position
  int pos_;

  // Behaves like |pos_| in the "> 0" case, but for near jumps to this label.
  int near_link_pos_;

  void bind_to(int pos)  {
    pos_ = -pos - 1;
    ASSERT(is_bound());
  }
  void link_to(int pos, Distance distance = kFar) {
    if (distance == kNear) {
      near_link_pos_ = pos + 1;
      ASSERT(is_near_linked());
    } else {
      pos_ = pos + 1;
      ASSERT(is_linked());
    }
  }

  friend class Assembler;
  friend class Displacement;
  friend class RegExpMacroAssemblerIrregexp;
};


enum SaveFPRegsMode { kDontSaveFPRegs, kSaveFPRegs };


// -----------------------------------------------------------------------------
// Relocation information


// Relocation information consists of the address (pc) of the datum
// to which the relocation information applies, the relocation mode
// (rmode), and an optional data field. The relocation mode may be
// "descriptive" and not indicate a need for relocation, but simply
// describe a property of the datum. Such rmodes are useful for GC
// and nice disassembly output.

class RelocInfo BASE_EMBEDDED {
 public:
  // The constant kNoPosition is used with the collecting of source positions
  // in the relocation information. Two types of source positions are collected
  // "position" (RelocMode position) and "statement position" (RelocMode
  // statement_position). The "position" is collected at places in the source
  // code which are of interest when making stack traces to pin-point the source
  // location of a stack frame as close as possible. The "statement position" is
  // collected at the beginning at each statement, and is used to indicate
  // possible break locations. kNoPosition is used to indicate an
  // invalid/uninitialized position value.
  static const int kNoPosition = -1;

  // This string is used to add padding comments to the reloc info in cases
  // where we are not sure to have enough space for patching in during
  // lazy deoptimization. This is the case if we have indirect calls for which
  // we do not normally record relocation info.
  static const char* const kFillerCommentString;

  // The minimum size of a comment is equal to three bytes for the extra tagged
  // pc + the tag for the data, and kPointerSize for the actual pointer to the
  // comment.
  static const int kMinRelocCommentSize = 3 + kPointerSize;

  // The maximum size for a call instruction including pc-jump.
  static const int kMaxCallSize = 6;

  // The maximum pc delta that will use the short encoding.
  static const int kMaxSmallPCDelta;

  enum Mode {
    // Please note the order is important (see IsCodeTarget, IsGCRelocMode).
    CODE_TARGET,  // Code target which is not any of the above.
    CODE_TARGET_WITH_ID,
    CONSTRUCT_CALL,  // code target that is a call to a JavaScript constructor.
    DEBUG_BREAK,  // Code target for the debugger statement.
    EMBEDDED_OBJECT,
    CELL,

    // Everything after runtime_entry (inclusive) is not GC'ed.
    RUNTIME_ENTRY,
    JS_RETURN,  // Marks start of the ExitJSFrame code.
    COMMENT,
    POSITION,  // See comment for kNoPosition above.
    STATEMENT_POSITION,  // See comment for kNoPosition above.
    DEBUG_BREAK_SLOT,  // Additional code inserted for debug break slot.
    EXTERNAL_REFERENCE,  // The address of an external C++ function.
    INTERNAL_REFERENCE,  // An address inside the same function.

    // Marks a constant pool. Only used on ARM.
    // It uses a custom noncompact encoding.
    CONST_POOL,

    // add more as needed
    // Pseudo-types
    NUMBER_OF_MODES,  // There are at most 15 modes with noncompact encoding.
    NONE32,  // never recorded 32-bit value
    NONE64,  // never recorded 64-bit value
    CODE_AGE_SEQUENCE,  // Not stored in RelocInfo array, used explictly by
                        // code aging.
    FIRST_REAL_RELOC_MODE = CODE_TARGET,
    LAST_REAL_RELOC_MODE = CONST_POOL,
    FIRST_PSEUDO_RELOC_MODE = CODE_AGE_SEQUENCE,
    LAST_PSEUDO_RELOC_MODE = CODE_AGE_SEQUENCE,
    LAST_CODE_ENUM = DEBUG_BREAK,
    LAST_GCED_ENUM = CELL,
    // Modes <= LAST_COMPACT_ENUM are guaranteed to have compact encoding.
    LAST_COMPACT_ENUM = CODE_TARGET_WITH_ID,
    LAST_STANDARD_NONCOMPACT_ENUM = INTERNAL_REFERENCE
  };


  RelocInfo() {}

  RelocInfo(byte* pc, Mode rmode, intptr_t data, Code* host)
      : pc_(pc), rmode_(rmode), data_(data), host_(host) {
  }
  RelocInfo(byte* pc, double data64)
      : pc_(pc), rmode_(NONE64), data64_(data64), host_(NULL) {
  }

  static inline bool IsRealRelocMode(Mode mode) {
    return mode >= FIRST_REAL_RELOC_MODE &&
        mode <= LAST_REAL_RELOC_MODE;
  }
  static inline bool IsPseudoRelocMode(Mode mode) {
    ASSERT(!IsRealRelocMode(mode));
    return mode >= FIRST_PSEUDO_RELOC_MODE &&
        mode <= LAST_PSEUDO_RELOC_MODE;
  }
  static inline bool IsConstructCall(Mode mode) {
    return mode == CONSTRUCT_CALL;
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
  // Is the relocation mode affected by GC?
  static inline bool IsGCRelocMode(Mode mode) {
    return mode <= LAST_GCED_ENUM;
  }
  static inline bool IsJSReturn(Mode mode) {
    return mode == JS_RETURN;
  }
  static inline bool IsComment(Mode mode) {
    return mode == COMMENT;
  }
  static inline bool IsConstPool(Mode mode) {
    return mode == CONST_POOL;
  }
  static inline bool IsPosition(Mode mode) {
    return mode == POSITION || mode == STATEMENT_POSITION;
  }
  static inline bool IsStatementPosition(Mode mode) {
    return mode == STATEMENT_POSITION;
  }
  static inline bool IsExternalReference(Mode mode) {
    return mode == EXTERNAL_REFERENCE;
  }
  static inline bool IsInternalReference(Mode mode) {
    return mode == INTERNAL_REFERENCE;
  }
  static inline bool IsDebugBreakSlot(Mode mode) {
    return mode == DEBUG_BREAK_SLOT;
  }
  static inline bool IsNone(Mode mode) {
    return mode == NONE32 || mode == NONE64;
  }
  static inline bool IsCodeAgeSequence(Mode mode) {
    return mode == CODE_AGE_SEQUENCE;
  }
  static inline int ModeMask(Mode mode) { return 1 << mode; }

  // Accessors
  byte* pc() const { return pc_; }
  void set_pc(byte* pc) { pc_ = pc; }
  Mode rmode() const {  return rmode_; }
  intptr_t data() const { return data_; }
  double data64() const { return data64_; }
  uint64_t raw_data64() {
    return BitCast<uint64_t>(data64_);
  }
  Code* host() const { return host_; }

  // Apply a relocation by delta bytes
  INLINE(void apply(intptr_t delta));

  // Is the pointer this relocation info refers to coded like a plain pointer
  // or is it strange in some way (e.g. relative or patched into a series of
  // instructions).
  bool IsCodedSpecially();

  // Read/modify the code target in the branch/call instruction
  // this relocation applies to;
  // can only be called if IsCodeTarget(rmode_) || IsRuntimeEntry(rmode_)
  INLINE(Address target_address());
  INLINE(void set_target_address(Address target,
                                 WriteBarrierMode mode = UPDATE_WRITE_BARRIER));
  INLINE(Object* target_object());
  INLINE(Handle<Object> target_object_handle(Assembler* origin));
  INLINE(void set_target_object(Object* target,
                                WriteBarrierMode mode = UPDATE_WRITE_BARRIER));
  INLINE(Address target_runtime_entry(Assembler* origin));
  INLINE(void set_target_runtime_entry(Address target,
                                       WriteBarrierMode mode =
                                           UPDATE_WRITE_BARRIER));
  INLINE(Cell* target_cell());
  INLINE(Handle<Cell> target_cell_handle());
  INLINE(void set_target_cell(Cell* cell,
                              WriteBarrierMode mode = UPDATE_WRITE_BARRIER));
  INLINE(Handle<Object> code_age_stub_handle(Assembler* origin));
  INLINE(Code* code_age_stub());
  INLINE(void set_code_age_stub(Code* stub));

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

  // Read/modify the reference in the instruction this relocation
  // applies to; can only be called if rmode_ is external_reference
  INLINE(Address target_reference());

  // Read/modify the address of a call instruction. This is used to relocate
  // the break points where straight-line code is patched with a call
  // instruction.
  INLINE(Address call_address());
  INLINE(void set_call_address(Address target));
  INLINE(Object* call_object());
  INLINE(void set_call_object(Object* target));
  INLINE(Object** call_object_address());

  // Wipe out a relocation to a fixed value, used for making snapshots
  // reproducible.
  INLINE(void WipeOut());

  template<typename StaticVisitor> inline void Visit(Heap* heap);
  inline void Visit(Isolate* isolate, ObjectVisitor* v);

  // Patch the code with some other code.
  void PatchCode(byte* instructions, int instruction_count);

  // Patch the code with a call.
  void PatchCodeWithCall(Address target, int guard_bytes);

  // Check whether this return sequence has been patched
  // with a call to the debugger.
  INLINE(bool IsPatchedReturnSequence());

  // Check whether this debug break slot has been patched with a call to the
  // debugger.
  INLINE(bool IsPatchedDebugBreakSlotSequence());

#ifdef DEBUG
  // Check whether the given code contains relocation information that
  // either is position-relative or movable by the garbage collector.
  static bool RequiresRelocation(const CodeDesc& desc);
#endif

#ifdef ENABLE_DISASSEMBLER
  // Printing
  static const char* RelocModeName(Mode rmode);
  void Print(Isolate* isolate, FILE* out);
#endif  // ENABLE_DISASSEMBLER
#ifdef VERIFY_HEAP
  void Verify();
#endif

  static const int kCodeTargetMask = (1 << (LAST_CODE_ENUM + 1)) - 1;
  static const int kPositionMask = 1 << POSITION | 1 << STATEMENT_POSITION;
  static const int kDataMask =
      (1 << CODE_TARGET_WITH_ID) | kPositionMask | (1 << COMMENT);
  static const int kApplyMask;  // Modes affected by apply. Depends on arch.

 private:
  // On ARM, note that pc_ is the address of the constant pool entry
  // to be relocated and not the address of the instruction
  // referencing the constant pool entry (except when rmode_ ==
  // comment).
  byte* pc_;
  Mode rmode_;
  union {
    intptr_t data_;
    double data64_;
  };
  Code* host_;
  // External-reference pointers are also split across instruction-pairs
  // on some platforms, but are accessed via indirect pointers. This location
  // provides a place for that pointer to exist naturally. Its address
  // is returned by RelocInfo::target_reference_address().
  Address reconstructed_adr_ptr_;
  friend class RelocIterator;
};


// RelocInfoWriter serializes a stream of relocation info. It writes towards
// lower addresses.
class RelocInfoWriter BASE_EMBEDDED {
 public:
  RelocInfoWriter() : pos_(NULL),
                      last_pc_(NULL),
                      last_id_(0),
                      last_position_(0) {}
  RelocInfoWriter(byte* pos, byte* pc) : pos_(pos),
                                         last_pc_(pc),
                                         last_id_(0),
                                         last_position_(0) {}

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
  // ExtraTag, VariableLengthPCJump, ExtraTag, pc_delta, ExtraTag, data_delta.
  // On ia32 and arm this is 1 + 4 + 1 + 1 + 1 + 4 = 12.
  // On x64 this is 1 + 4 + 1 + 1 + 1 + 8 == 16;
  // Here we use the maximum of the two.
  static const int kMaxSize = 16;

 private:
  inline uint32_t WriteVariableLengthPCJump(uint32_t pc_delta);
  inline void WriteTaggedPC(uint32_t pc_delta, int tag);
  inline void WriteExtraTaggedPC(uint32_t pc_delta, int extra_tag);
  inline void WriteExtraTaggedIntData(int data_delta, int top_tag);
  inline void WriteExtraTaggedConstPoolData(int data);
  inline void WriteExtraTaggedData(intptr_t data_delta, int top_tag);
  inline void WriteTaggedData(intptr_t data_delta, int tag);
  inline void WriteExtraTag(int extra_tag, int top_tag);

  byte* pos_;
  byte* last_pc_;
  int last_id_;
  int last_position_;
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

  // Iteration
  bool done() const { return done_; }
  void next();

  // Return pointer valid until next next().
  RelocInfo* rinfo() {
    ASSERT(!done());
    return &rinfo_;
  }

 private:
  // Advance* moves the position before/after reading.
  // *Read* reads from current byte(s) into rinfo_.
  // *Get* just reads and returns info on current byte.
  void Advance(int bytes = 1) { pos_ -= bytes; }
  int AdvanceGetTag();
  int GetExtraTag();
  int GetTopTag();
  void ReadTaggedPC();
  void AdvanceReadPC();
  void AdvanceReadId();
  void AdvanceReadConstPoolData();
  void AdvanceReadPosition();
  void AdvanceReadData();
  void AdvanceReadVariableLengthPCJump();
  int GetLocatableTypeTag();
  void ReadTaggedId();
  void ReadTaggedPosition();

  // If the given mode is wanted, set it in rinfo_ and return true.
  // Else return false. Used for efficiently skipping unwanted modes.
  bool SetMode(RelocInfo::Mode mode) {
    return (mode_mask_ & (1 << mode)) ? (rinfo_.rmode_ = mode, true) : false;
  }

  byte* pos_;
  byte* end_;
  byte* code_age_sequence_;
  RelocInfo rinfo_;
  bool done_;
  int mode_mask_;
  int last_id_;
  int last_position_;
  DISALLOW_COPY_AND_ASSIGN(RelocIterator);
};


//------------------------------------------------------------------------------
// External function

//----------------------------------------------------------------------------
class IC_Utility;
class SCTableReference;
#ifdef ENABLE_DEBUGGER_SUPPORT
class Debug_Address;
#endif


// An ExternalReference represents a C++ address used in the generated
// code. All references to C++ functions and variables must be encapsulated in
// an ExternalReference instance. This is done in order to track the origin of
// all external references in the code so that they can be bound to the correct
// addresses when deserializing a heap.
class ExternalReference BASE_EMBEDDED {
 public:
  // Used in the simulator to support different native api calls.
  enum Type {
    // Builtin call.
    // MaybeObject* f(v8::internal::Arguments).
    BUILTIN_CALL,  // default

    // Builtin that takes float arguments and returns an int.
    // int f(double, double).
    BUILTIN_COMPARE_CALL,

    // Builtin call that returns floating point.
    // double f(double, double).
    BUILTIN_FP_FP_CALL,

    // Builtin call that returns floating point.
    // double f(double).
    BUILTIN_FP_CALL,

    // Builtin call that returns floating point.
    // double f(double, int).
    BUILTIN_FP_INT_CALL,

    // Direct call to API function callback.
    // void f(v8::FunctionCallbackInfo&)
    DIRECT_API_CALL,

    // Call to function callback via InvokeFunctionCallback.
    // void f(v8::FunctionCallbackInfo&, v8::FunctionCallback)
    PROFILING_API_CALL,

    // Direct call to accessor getter callback.
    // void f(Local<String> property, PropertyCallbackInfo& info)
    DIRECT_GETTER_CALL,

    // Call to accessor getter callback via InvokeAccessorGetterCallback.
    // void f(Local<String> property, PropertyCallbackInfo& info,
    //     AccessorGetterCallback callback)
    PROFILING_GETTER_CALL
  };

  static void SetUp();
  static void InitializeMathExpData();
  static void TearDownMathExpData();

  typedef void* ExternalReferenceRedirector(void* original, Type type);

  ExternalReference() : address_(NULL) {}

  ExternalReference(Builtins::CFunctionId id, Isolate* isolate);

  ExternalReference(ApiFunction* ptr, Type type, Isolate* isolate);

  ExternalReference(Builtins::Name name, Isolate* isolate);

  ExternalReference(Runtime::FunctionId id, Isolate* isolate);

  ExternalReference(const Runtime::Function* f, Isolate* isolate);

  ExternalReference(const IC_Utility& ic_utility, Isolate* isolate);

#ifdef ENABLE_DEBUGGER_SUPPORT
  ExternalReference(const Debug_Address& debug_address, Isolate* isolate);
#endif

  explicit ExternalReference(StatsCounter* counter);

  ExternalReference(Isolate::AddressId id, Isolate* isolate);

  explicit ExternalReference(const SCTableReference& table_ref);

  // Isolate as an external reference.
  static ExternalReference isolate_address(Isolate* isolate);

  // One-of-a-kind references. These references are not part of a general
  // pattern. This means that they have to be added to the
  // ExternalReferenceTable in serialize.cc manually.

  static ExternalReference incremental_marking_record_write_function(
      Isolate* isolate);
  static ExternalReference incremental_evacuation_record_write_function(
      Isolate* isolate);
  static ExternalReference store_buffer_overflow_function(
      Isolate* isolate);
  static ExternalReference flush_icache_function(Isolate* isolate);
  static ExternalReference perform_gc_function(Isolate* isolate);
  static ExternalReference delete_handle_scope_extensions(Isolate* isolate);

  static ExternalReference get_date_field_function(Isolate* isolate);
  static ExternalReference date_cache_stamp(Isolate* isolate);

  static ExternalReference get_make_code_young_function(Isolate* isolate);
  static ExternalReference get_mark_code_as_executed_function(Isolate* isolate);

  // Deoptimization support.
  static ExternalReference new_deoptimizer_function(Isolate* isolate);
  static ExternalReference compute_output_frames_function(Isolate* isolate);

  // Log support.
  static ExternalReference log_enter_external_function(Isolate* isolate);
  static ExternalReference log_leave_external_function(Isolate* isolate);

  // Static data in the keyed lookup cache.
  static ExternalReference keyed_lookup_cache_keys(Isolate* isolate);
  static ExternalReference keyed_lookup_cache_field_offsets(Isolate* isolate);

  // Static variable Heap::roots_array_start()
  static ExternalReference roots_array_start(Isolate* isolate);

  // Static variable Heap::allocation_sites_list_address()
  static ExternalReference allocation_sites_list_address(Isolate* isolate);

  // Static variable StackGuard::address_of_jslimit()
  static ExternalReference address_of_stack_limit(Isolate* isolate);

  // Static variable StackGuard::address_of_real_jslimit()
  static ExternalReference address_of_real_stack_limit(Isolate* isolate);

  // Static variable RegExpStack::limit_address()
  static ExternalReference address_of_regexp_stack_limit(Isolate* isolate);

  // Static variables for RegExp.
  static ExternalReference address_of_static_offsets_vector(Isolate* isolate);
  static ExternalReference address_of_regexp_stack_memory_address(
      Isolate* isolate);
  static ExternalReference address_of_regexp_stack_memory_size(
      Isolate* isolate);

  // Static variable Heap::NewSpaceStart()
  static ExternalReference new_space_start(Isolate* isolate);
  static ExternalReference new_space_mask(Isolate* isolate);
  static ExternalReference heap_always_allocate_scope_depth(Isolate* isolate);
  static ExternalReference new_space_mark_bits(Isolate* isolate);

  // Write barrier.
  static ExternalReference store_buffer_top(Isolate* isolate);

  // Used for fast allocation in generated code.
  static ExternalReference new_space_allocation_top_address(Isolate* isolate);
  static ExternalReference new_space_allocation_limit_address(Isolate* isolate);
  static ExternalReference old_pointer_space_allocation_top_address(
      Isolate* isolate);
  static ExternalReference old_pointer_space_allocation_limit_address(
      Isolate* isolate);
  static ExternalReference old_data_space_allocation_top_address(
      Isolate* isolate);
  static ExternalReference old_data_space_allocation_limit_address(
      Isolate* isolate);
  static ExternalReference new_space_high_promotion_mode_active_address(
      Isolate* isolate);

  static ExternalReference mod_two_doubles_operation(Isolate* isolate);
  static ExternalReference power_double_double_function(Isolate* isolate);
  static ExternalReference power_double_int_function(Isolate* isolate);

  static ExternalReference handle_scope_next_address(Isolate* isolate);
  static ExternalReference handle_scope_limit_address(Isolate* isolate);
  static ExternalReference handle_scope_level_address(Isolate* isolate);

  static ExternalReference scheduled_exception_address(Isolate* isolate);
  static ExternalReference address_of_pending_message_obj(Isolate* isolate);
  static ExternalReference address_of_has_pending_message(Isolate* isolate);
  static ExternalReference address_of_pending_message_script(Isolate* isolate);

  // Static variables containing common double constants.
  static ExternalReference address_of_min_int();
  static ExternalReference address_of_one_half();
  static ExternalReference address_of_minus_one_half();
  static ExternalReference address_of_minus_zero();
  static ExternalReference address_of_zero();
  static ExternalReference address_of_uint8_max_value();
  static ExternalReference address_of_negative_infinity();
  static ExternalReference address_of_canonical_non_hole_nan();
  static ExternalReference address_of_the_hole_nan();
  static ExternalReference address_of_uint32_bias();

  static ExternalReference math_log_double_function(Isolate* isolate);

  static ExternalReference math_exp_constants(int constant_index);
  static ExternalReference math_exp_log_table();

  static ExternalReference page_flags(Page* page);

  static ExternalReference ForDeoptEntry(Address entry);

  static ExternalReference cpu_features();

  Address address() const { return reinterpret_cast<Address>(address_); }

#ifdef ENABLE_DEBUGGER_SUPPORT
  // Function Debug::Break()
  static ExternalReference debug_break(Isolate* isolate);

  // Used to check if single stepping is enabled in generated code.
  static ExternalReference debug_step_in_fp_address(Isolate* isolate);
#endif

#ifndef V8_INTERPRETED_REGEXP
  // C functions called from RegExp generated code.

  // Function NativeRegExpMacroAssembler::CaseInsensitiveCompareUC16()
  static ExternalReference re_case_insensitive_compare_uc16(Isolate* isolate);

  // Function RegExpMacroAssembler*::CheckStackGuardState()
  static ExternalReference re_check_stack_guard_state(Isolate* isolate);

  // Function NativeRegExpMacroAssembler::GrowStack()
  static ExternalReference re_grow_stack(Isolate* isolate);

  // byte NativeRegExpMacroAssembler::word_character_bitmap
  static ExternalReference re_word_character_map();

#endif

  // This lets you register a function that rewrites all external references.
  // Used by the ARM simulator to catch calls to external references.
  static void set_redirector(Isolate* isolate,
                             ExternalReferenceRedirector* redirector) {
    // We can't stack them.
    ASSERT(isolate->external_reference_redirector() == NULL);
    isolate->set_external_reference_redirector(
        reinterpret_cast<ExternalReferenceRedirectorPointer*>(redirector));
  }

  static ExternalReference stress_deopt_count(Isolate* isolate);

  bool operator==(const ExternalReference& other) const {
    return address_ == other.address_;
  }

  bool operator!=(const ExternalReference& other) const {
    return !(*this == other);
  }

 private:
  explicit ExternalReference(void* address)
      : address_(address) {}

  static void* Redirect(Isolate* isolate,
                        void* address,
                        Type type = ExternalReference::BUILTIN_CALL) {
    ExternalReferenceRedirector* redirector =
        reinterpret_cast<ExternalReferenceRedirector*>(
            isolate->external_reference_redirector());
    if (redirector == NULL) return address;
    void* answer = (*redirector)(address, type);
    return answer;
  }

  static void* Redirect(Isolate* isolate,
                        Address address_arg,
                        Type type = ExternalReference::BUILTIN_CALL) {
    ExternalReferenceRedirector* redirector =
        reinterpret_cast<ExternalReferenceRedirector*>(
            isolate->external_reference_redirector());
    void* address = reinterpret_cast<void*>(address_arg);
    void* answer = (redirector == NULL) ?
                   address :
                   (*redirector)(address, type);
    return answer;
  }

  void* address_;
};


// -----------------------------------------------------------------------------
// Position recording support

struct PositionState {
  PositionState() : current_position(RelocInfo::kNoPosition),
                    written_position(RelocInfo::kNoPosition),
                    current_statement_position(RelocInfo::kNoPosition),
                    written_statement_position(RelocInfo::kNoPosition) {}

  int current_position;
  int written_position;

  int current_statement_position;
  int written_statement_position;
};


class PositionsRecorder BASE_EMBEDDED {
 public:
  explicit PositionsRecorder(Assembler* assembler)
      : assembler_(assembler) {
#ifdef ENABLE_GDB_JIT_INTERFACE
    gdbjit_lineinfo_ = NULL;
#endif
    jit_handler_data_ = NULL;
  }

#ifdef ENABLE_GDB_JIT_INTERFACE
  ~PositionsRecorder() {
    delete gdbjit_lineinfo_;
  }

  void StartGDBJITLineInfoRecording() {
    if (FLAG_gdbjit) {
      gdbjit_lineinfo_ = new GDBJITLineInfo();
    }
  }

  GDBJITLineInfo* DetachGDBJITLineInfo() {
    GDBJITLineInfo* lineinfo = gdbjit_lineinfo_;
    gdbjit_lineinfo_ = NULL;  // To prevent deallocation in destructor.
    return lineinfo;
  }
#endif
  void AttachJITHandlerData(void* user_data) {
    jit_handler_data_ = user_data;
  }

  void* DetachJITHandlerData() {
    void* old_data = jit_handler_data_;
    jit_handler_data_ = NULL;
    return old_data;
  }
  // Set current position to pos.
  void RecordPosition(int pos);

  // Set current statement position to pos.
  void RecordStatementPosition(int pos);

  // Write recorded positions to relocation information.
  bool WriteRecordedPositions();

  int current_position() const { return state_.current_position; }

  int current_statement_position() const {
    return state_.current_statement_position;
  }

 private:
  Assembler* assembler_;
  PositionState state_;
#ifdef ENABLE_GDB_JIT_INTERFACE
  GDBJITLineInfo* gdbjit_lineinfo_;
#endif

  // Currently jit_handler_data_ is used to store JITHandler-specific data
  // over the lifetime of a PositionsRecorder
  void* jit_handler_data_;
  friend class PreservePositionScope;

  DISALLOW_COPY_AND_ASSIGN(PositionsRecorder);
};


class PreservePositionScope BASE_EMBEDDED {
 public:
  explicit PreservePositionScope(PositionsRecorder* positions_recorder)
      : positions_recorder_(positions_recorder),
        saved_state_(positions_recorder->state_) {}

  ~PreservePositionScope() {
    positions_recorder_->state_ = saved_state_;
  }

 private:
  PositionsRecorder* positions_recorder_;
  const PositionState saved_state_;

  DISALLOW_COPY_AND_ASSIGN(PreservePositionScope);
};


// -----------------------------------------------------------------------------
// Utility functions

inline int NumberOfBitsSet(uint32_t x) {
  unsigned int num_bits_set;
  for (num_bits_set = 0; x; x >>= 1) {
    num_bits_set += x & 1;
  }
  return num_bits_set;
}

bool EvalComparison(Token::Value op, double op1, double op2);

// Computes pow(x, y) with the special cases in the spec for Math.pow.
double power_helper(double x, double y);
double power_double_int(double x, int y);
double power_double_double(double x, double y);

// Helper class for generating code or data associated with the code
// right after a call instruction. As an example this can be used to
// generate safepoint data after calls for crankshaft.
class CallWrapper {
 public:
  CallWrapper() { }
  virtual ~CallWrapper() { }
  // Called just before emitting a call. Argument is the size of the generated
  // call code.
  virtual void BeforeCall(int call_size) const = 0;
  // Called just after emitting a call, i.e., at the return site for the call.
  virtual void AfterCall() const = 0;
};

class NullCallWrapper : public CallWrapper {
 public:
  NullCallWrapper() { }
  virtual ~NullCallWrapper() { }
  virtual void BeforeCall(int call_size) const { }
  virtual void AfterCall() const { }
};

} }  // namespace v8::internal

#endif  // V8_ASSEMBLER_H_
