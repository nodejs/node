// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_H_
#define V8_COMPILER_H_

#include "src/allocation.h"
#include "src/ast.h"
#include "src/bailout-reason.h"
#include "src/zone.h"

namespace v8 {
namespace internal {

class AstValueFactory;
class HydrogenCodeStub;
class ParseInfo;
class ScriptData;

struct OffsetRange {
  OffsetRange(int from, int to) : from(from), to(to) {}
  int from;
  int to;
};


// This class encapsulates encoding and decoding of sources positions from
// which hydrogen values originated.
// When FLAG_track_hydrogen_positions is set this object encodes the
// identifier of the inlining and absolute offset from the start of the
// inlined function.
// When the flag is not set we simply track absolute offset from the
// script start.
class SourcePosition {
 public:
  static SourcePosition Unknown() {
    return SourcePosition::FromRaw(kNoPosition);
  }

  bool IsUnknown() const { return value_ == kNoPosition; }

  uint32_t position() const { return PositionField::decode(value_); }
  void set_position(uint32_t position) {
    if (FLAG_hydrogen_track_positions) {
      value_ = static_cast<uint32_t>(PositionField::update(value_, position));
    } else {
      value_ = position;
    }
  }

  uint32_t inlining_id() const { return InliningIdField::decode(value_); }
  void set_inlining_id(uint32_t inlining_id) {
    if (FLAG_hydrogen_track_positions) {
      value_ =
          static_cast<uint32_t>(InliningIdField::update(value_, inlining_id));
    }
  }

  uint32_t raw() const { return value_; }

 private:
  static const uint32_t kNoPosition =
      static_cast<uint32_t>(RelocInfo::kNoPosition);
  typedef BitField<uint32_t, 0, 9> InliningIdField;

  // Offset from the start of the inlined function.
  typedef BitField<uint32_t, 9, 23> PositionField;

  friend class HPositionInfo;
  friend class Deoptimizer;

  static SourcePosition FromRaw(uint32_t raw_position) {
    SourcePosition position;
    position.value_ = raw_position;
    return position;
  }

  // If FLAG_hydrogen_track_positions is set contains bitfields InliningIdField
  // and PositionField.
  // Otherwise contains absolute offset from the script start.
  uint32_t value_;
};


std::ostream& operator<<(std::ostream& os, const SourcePosition& p);


struct InlinedFunctionInfo {
  InlinedFunctionInfo(int parent_id, SourcePosition inline_position,
                      int script_id, int start_position)
      : parent_id(parent_id),
        inline_position(inline_position),
        script_id(script_id),
        start_position(start_position) {}
  int parent_id;
  SourcePosition inline_position;
  int script_id;
  int start_position;
  std::vector<size_t> deopt_pc_offsets;

  static const int kNoParentId = -1;
};


// CompilationInfo encapsulates some information known at compile time.  It
// is constructed based on the resources available at compile-time.
class CompilationInfo {
 public:
  // Various configuration flags for a compilation, as well as some properties
  // of the compiled code produced by a compilation.
  enum Flag {
    kDeferredCalling = 1 << 0,
    kNonDeferredCalling = 1 << 1,
    kSavesCallerDoubles = 1 << 2,
    kRequiresFrame = 1 << 3,
    kMustNotHaveEagerFrame = 1 << 4,
    kDeoptimizationSupport = 1 << 5,
    kDebug = 1 << 6,
    kCompilingForDebugging = 1 << 7,
    kSerializing = 1 << 8,
    kContextSpecializing = 1 << 9,
    kInliningEnabled = 1 << 10,
    kTypingEnabled = 1 << 11,
    kDisableFutureOptimization = 1 << 12,
    kSplittingEnabled = 1 << 13,
    kBuiltinInliningEnabled = 1 << 14,
    kTypeFeedbackEnabled = 1 << 15
  };

  explicit CompilationInfo(ParseInfo* parse_info);
  CompilationInfo(CodeStub* stub, Isolate* isolate, Zone* zone);
  virtual ~CompilationInfo();

  ParseInfo* parse_info() const { return parse_info_; }

  // -----------------------------------------------------------
  // TODO(titzer): inline and delete accessors of ParseInfo
  // -----------------------------------------------------------
  Handle<Script> script() const;
  bool is_eval() const;
  bool is_native() const;
  bool is_module() const;
  LanguageMode language_mode() const;
  Handle<JSFunction> closure() const;
  FunctionLiteral* function() const;
  Scope* scope() const;
  Handle<Context> context() const;
  Handle<SharedFunctionInfo> shared_info() const;
  bool has_shared_info() const;
  // -----------------------------------------------------------

  Isolate* isolate() const {
    return isolate_;
  }
  Zone* zone() { return zone_; }
  bool is_osr() const { return !osr_ast_id_.IsNone(); }
  Handle<Code> code() const { return code_; }
  CodeStub* code_stub() const { return code_stub_; }
  BailoutId osr_ast_id() const { return osr_ast_id_; }
  Handle<Code> unoptimized_code() const { return unoptimized_code_; }
  int opt_count() const { return opt_count_; }
  int num_parameters() const;
  int num_heap_slots() const;
  Code::Flags flags() const;
  bool has_scope() const { return scope() != nullptr; }

  void set_parameter_count(int parameter_count) {
    DCHECK(IsStub());
    parameter_count_ = parameter_count;
  }

  bool is_tracking_positions() const { return track_positions_; }

  bool is_calling() const {
    return GetFlag(kDeferredCalling) || GetFlag(kNonDeferredCalling);
  }

  void MarkAsDeferredCalling() { SetFlag(kDeferredCalling); }

  bool is_deferred_calling() const { return GetFlag(kDeferredCalling); }

  void MarkAsNonDeferredCalling() { SetFlag(kNonDeferredCalling); }

  bool is_non_deferred_calling() const { return GetFlag(kNonDeferredCalling); }

  void MarkAsSavesCallerDoubles() { SetFlag(kSavesCallerDoubles); }

  bool saves_caller_doubles() const { return GetFlag(kSavesCallerDoubles); }

  void MarkAsRequiresFrame() { SetFlag(kRequiresFrame); }

  bool requires_frame() const { return GetFlag(kRequiresFrame); }

  void MarkMustNotHaveEagerFrame() { SetFlag(kMustNotHaveEagerFrame); }

  bool GetMustNotHaveEagerFrame() const {
    return GetFlag(kMustNotHaveEagerFrame);
  }

  void MarkAsDebug() { SetFlag(kDebug); }

  bool is_debug() const { return GetFlag(kDebug); }

  void PrepareForSerializing() { SetFlag(kSerializing); }

  bool will_serialize() const { return GetFlag(kSerializing); }

  void MarkAsContextSpecializing() { SetFlag(kContextSpecializing); }

  bool is_context_specializing() const { return GetFlag(kContextSpecializing); }

  void MarkAsTypeFeedbackEnabled() { SetFlag(kTypeFeedbackEnabled); }

  bool is_type_feedback_enabled() const {
    return GetFlag(kTypeFeedbackEnabled);
  }

  void MarkAsInliningEnabled() { SetFlag(kInliningEnabled); }

  bool is_inlining_enabled() const { return GetFlag(kInliningEnabled); }

  void MarkAsBuiltinInliningEnabled() { SetFlag(kBuiltinInliningEnabled); }

  bool is_builtin_inlining_enabled() const {
    return GetFlag(kBuiltinInliningEnabled);
  }

  void MarkAsTypingEnabled() { SetFlag(kTypingEnabled); }

  bool is_typing_enabled() const { return GetFlag(kTypingEnabled); }

  void MarkAsSplittingEnabled() { SetFlag(kSplittingEnabled); }

  bool is_splitting_enabled() const { return GetFlag(kSplittingEnabled); }

  bool IsCodePreAgingActive() const {
    return FLAG_optimize_for_size && FLAG_age_code && !will_serialize() &&
           !is_debug();
  }

  void EnsureFeedbackVector();
  Handle<TypeFeedbackVector> feedback_vector() const {
    return feedback_vector_;
  }
  void SetCode(Handle<Code> code) { code_ = code; }

  void MarkCompilingForDebugging() { SetFlag(kCompilingForDebugging); }
  bool IsCompilingForDebugging() { return GetFlag(kCompilingForDebugging); }
  void MarkNonOptimizable() {
    SetMode(CompilationInfo::NONOPT);
  }

  bool ShouldTrapOnDeopt() const {
    return (FLAG_trap_on_deopt && IsOptimizing()) ||
        (FLAG_trap_on_stub_deopt && IsStub());
  }

  bool has_global_object() const {
    return !closure().is_null() &&
        (closure()->context()->global_object() != NULL);
  }

  GlobalObject* global_object() const {
    return has_global_object() ? closure()->context()->global_object() : NULL;
  }

  // Accessors for the different compilation modes.
  bool IsOptimizing() const { return mode_ == OPTIMIZE; }
  bool IsOptimizable() const { return mode_ == BASE; }
  bool IsStub() const { return mode_ == STUB; }
  void SetOptimizing(BailoutId osr_ast_id, Handle<Code> unoptimized) {
    DCHECK(!shared_info().is_null());
    SetMode(OPTIMIZE);
    osr_ast_id_ = osr_ast_id;
    unoptimized_code_ = unoptimized;
    optimization_id_ = isolate()->NextOptimizationId();
  }

  void SetStub(CodeStub* code_stub) {
    SetMode(STUB);
    code_stub_ = code_stub;
  }

  // Deoptimization support.
  bool HasDeoptimizationSupport() const {
    return GetFlag(kDeoptimizationSupport);
  }
  void EnableDeoptimizationSupport() {
    DCHECK(IsOptimizable());
    SetFlag(kDeoptimizationSupport);
  }

  // Determines whether or not to insert a self-optimization header.
  bool ShouldSelfOptimize();

  void set_deferred_handles(DeferredHandles* deferred_handles) {
    DCHECK(deferred_handles_ == NULL);
    deferred_handles_ = deferred_handles;
  }

  ZoneList<Handle<HeapObject> >* dependencies(
      DependentCode::DependencyGroup group) {
    if (dependencies_[group] == NULL) {
      dependencies_[group] = new(zone_) ZoneList<Handle<HeapObject> >(2, zone_);
    }
    return dependencies_[group];
  }

  void CommitDependencies(Handle<Code> code);

  void RollbackDependencies();

  void ReopenHandlesInNewHandleScope() {
    unoptimized_code_ = Handle<Code>(*unoptimized_code_);
  }

  void AbortOptimization(BailoutReason reason) {
    DCHECK(reason != kNoReason);
    if (bailout_reason_ == kNoReason) bailout_reason_ = reason;
    SetFlag(kDisableFutureOptimization);
  }

  void RetryOptimization(BailoutReason reason) {
    DCHECK(reason != kNoReason);
    if (GetFlag(kDisableFutureOptimization)) return;
    bailout_reason_ = reason;
  }

  BailoutReason bailout_reason() const { return bailout_reason_; }

  int prologue_offset() const {
    DCHECK_NE(Code::kPrologueOffsetNotSet, prologue_offset_);
    return prologue_offset_;
  }

  void set_prologue_offset(int prologue_offset) {
    DCHECK_EQ(Code::kPrologueOffsetNotSet, prologue_offset_);
    prologue_offset_ = prologue_offset;
  }

  // Adds offset range [from, to) where fp register does not point
  // to the current frame base. Used in CPU profiler to detect stack
  // samples where top frame is not set up.
  inline void AddNoFrameRange(int from, int to) {
    if (no_frame_ranges_) no_frame_ranges_->Add(OffsetRange(from, to));
  }

  List<OffsetRange>* ReleaseNoFrameRanges() {
    List<OffsetRange>* result = no_frame_ranges_;
    no_frame_ranges_ = NULL;
    return result;
  }

  int start_position_for(uint32_t inlining_id) {
    return inlined_function_infos_.at(inlining_id).start_position;
  }
  const std::vector<InlinedFunctionInfo>& inlined_function_infos() {
    return inlined_function_infos_;
  }

  void LogDeoptCallPosition(int pc_offset, int inlining_id);
  int TraceInlinedFunction(Handle<SharedFunctionInfo> shared,
                           SourcePosition position, int pareint_id);

  Handle<Foreign> object_wrapper() {
    if (object_wrapper_.is_null()) {
      object_wrapper_ =
          isolate()->factory()->NewForeign(reinterpret_cast<Address>(this));
    }
    return object_wrapper_;
  }

  void AbortDueToDependencyChange() {
    DCHECK(!OptimizingCompilerThread::IsOptimizerThread(isolate()));
    aborted_due_to_dependency_change_ = true;
  }

  bool HasAbortedDueToDependencyChange() const {
    DCHECK(!OptimizingCompilerThread::IsOptimizerThread(isolate()));
    return aborted_due_to_dependency_change_;
  }

  bool HasSameOsrEntry(Handle<JSFunction> function, BailoutId osr_ast_id) {
    return osr_ast_id_ == osr_ast_id && function.is_identical_to(closure());
  }

  int optimization_id() const { return optimization_id_; }

  int osr_expr_stack_height() { return osr_expr_stack_height_; }
  void set_osr_expr_stack_height(int height) {
    DCHECK(height >= 0);
    osr_expr_stack_height_ = height;
  }

#if DEBUG
  void PrintAstForTesting();
#endif

  bool is_simple_parameter_list();

 protected:
  ParseInfo* parse_info_;

  void DisableFutureOptimization() {
    if (GetFlag(kDisableFutureOptimization) && has_shared_info()) {
      shared_info()->DisableOptimization(bailout_reason());
    }
  }

 private:
  // Compilation mode.
  // BASE is generated by the full codegen, optionally prepared for bailouts.
  // OPTIMIZE is optimized code generated by the Hydrogen-based backend.
  // NONOPT is generated by the full codegen and is not prepared for
  //   recompilation/bailouts.  These functions are never recompiled.
  enum Mode {
    BASE,
    OPTIMIZE,
    NONOPT,
    STUB
  };

  CompilationInfo(ParseInfo* parse_info, CodeStub* code_stub, Mode mode,
                  Isolate* isolate, Zone* zone);

  Isolate* isolate_;

  void SetMode(Mode mode) {
    mode_ = mode;
  }

  void SetFlag(Flag flag) { flags_ |= flag; }

  void SetFlag(Flag flag, bool value) {
    flags_ = value ? flags_ | flag : flags_ & ~flag;
  }

  bool GetFlag(Flag flag) const { return (flags_ & flag) != 0; }

  unsigned flags_;

  // For compiled stubs, the stub object
  CodeStub* code_stub_;
  // The compiled code.
  Handle<Code> code_;

  // Used by codegen, ultimately kept rooted by the SharedFunctionInfo.
  Handle<TypeFeedbackVector> feedback_vector_;

  // Compilation mode flag and whether deoptimization is allowed.
  Mode mode_;
  BailoutId osr_ast_id_;
  // The unoptimized code we patched for OSR may not be the shared code
  // afterwards, since we may need to compile it again to include deoptimization
  // data.  Keep track which code we patched.
  Handle<Code> unoptimized_code_;

  // The zone from which the compilation pipeline working on this
  // CompilationInfo allocates.
  Zone* zone_;

  DeferredHandles* deferred_handles_;

  ZoneList<Handle<HeapObject> >* dependencies_[DependentCode::kGroupCount];

  BailoutReason bailout_reason_;

  int prologue_offset_;

  List<OffsetRange>* no_frame_ranges_;
  std::vector<InlinedFunctionInfo> inlined_function_infos_;
  bool track_positions_;

  // A copy of shared_info()->opt_count() to avoid handle deref
  // during graph optimization.
  int opt_count_;

  // Number of parameters used for compilation of stubs that require arguments.
  int parameter_count_;

  Handle<Foreign> object_wrapper_;

  int optimization_id_;

  // This flag is used by the main thread to track whether this compilation
  // should be abandoned due to dependency change.
  bool aborted_due_to_dependency_change_;

  int osr_expr_stack_height_;

  DISALLOW_COPY_AND_ASSIGN(CompilationInfo);
};


// A wrapper around a CompilationInfo that detaches the Handles from
// the underlying DeferredHandleScope and stores them in info_ on
// destruction.
class CompilationHandleScope BASE_EMBEDDED {
 public:
  explicit CompilationHandleScope(CompilationInfo* info)
      : deferred_(info->isolate()), info_(info) {}
  ~CompilationHandleScope() {
    info_->set_deferred_handles(deferred_.Detach());
  }

 private:
  DeferredHandleScope deferred_;
  CompilationInfo* info_;
};


class HGraph;
class HOptimizedGraphBuilder;
class LChunk;

// A helper class that calls the three compilation phases in
// Crankshaft and keeps track of its state.  The three phases
// CreateGraph, OptimizeGraph and GenerateAndInstallCode can either
// fail, bail-out to the full code generator or succeed.  Apart from
// their return value, the status of the phase last run can be checked
// using last_status().
class OptimizedCompileJob: public ZoneObject {
 public:
  explicit OptimizedCompileJob(CompilationInfo* info)
      : info_(info),
        graph_builder_(NULL),
        graph_(NULL),
        chunk_(NULL),
        last_status_(FAILED),
        awaiting_install_(false) { }

  enum Status {
    FAILED, BAILED_OUT, SUCCEEDED
  };

  MUST_USE_RESULT Status CreateGraph();
  MUST_USE_RESULT Status OptimizeGraph();
  MUST_USE_RESULT Status GenerateCode();

  Status last_status() const { return last_status_; }
  CompilationInfo* info() const { return info_; }
  Isolate* isolate() const { return info()->isolate(); }

  Status RetryOptimization(BailoutReason reason) {
    info_->RetryOptimization(reason);
    return SetLastStatus(BAILED_OUT);
  }

  Status AbortOptimization(BailoutReason reason) {
    info_->AbortOptimization(reason);
    return SetLastStatus(BAILED_OUT);
  }

  void WaitForInstall() {
    DCHECK(info_->is_osr());
    awaiting_install_ = true;
  }

  bool IsWaitingForInstall() { return awaiting_install_; }

 private:
  CompilationInfo* info_;
  HOptimizedGraphBuilder* graph_builder_;
  HGraph* graph_;
  LChunk* chunk_;
  base::TimeDelta time_taken_to_create_graph_;
  base::TimeDelta time_taken_to_optimize_;
  base::TimeDelta time_taken_to_codegen_;
  Status last_status_;
  bool awaiting_install_;

  MUST_USE_RESULT Status SetLastStatus(Status status) {
    last_status_ = status;
    return last_status_;
  }
  void RecordOptimizationStats();

  struct Timer {
    Timer(OptimizedCompileJob* job, base::TimeDelta* location)
        : job_(job), location_(location) {
      DCHECK(location_ != NULL);
      timer_.Start();
    }

    ~Timer() {
      *location_ += timer_.Elapsed();
    }

    OptimizedCompileJob* job_;
    base::ElapsedTimer timer_;
    base::TimeDelta* location_;
  };
};


// The V8 compiler
//
// General strategy: Source code is translated into an anonymous function w/o
// parameters which then can be executed. If the source code contains other
// functions, they will be compiled and allocated as part of the compilation
// of the source code.

// Please note this interface returns shared function infos.  This means you
// need to call Factory::NewFunctionFromSharedFunctionInfo before you have a
// real function with a context.

class Compiler : public AllStatic {
 public:
  MUST_USE_RESULT static MaybeHandle<Code> GetUnoptimizedCode(
      Handle<JSFunction> function);
  MUST_USE_RESULT static MaybeHandle<Code> GetLazyCode(
      Handle<JSFunction> function);
  MUST_USE_RESULT static MaybeHandle<Code> GetUnoptimizedCode(
      Handle<SharedFunctionInfo> shared);
  MUST_USE_RESULT static MaybeHandle<Code> GetDebugCode(
      Handle<JSFunction> function);

  // Parser::Parse, then Compiler::Analyze.
  static bool ParseAndAnalyze(ParseInfo* info);
  // Rewrite, analyze scopes, and renumber.
  static bool Analyze(ParseInfo* info);
  // Adds deoptimization support, requires ParseAndAnalyze.
  static bool EnsureDeoptimizationSupport(CompilationInfo* info);

  static bool EnsureCompiled(Handle<JSFunction> function,
                             ClearExceptionFlag flag);

  static void CompileForLiveEdit(Handle<Script> script);

  // Compile a String source within a context for eval.
  MUST_USE_RESULT static MaybeHandle<JSFunction> GetFunctionFromEval(
      Handle<String> source, Handle<SharedFunctionInfo> outer_info,
      Handle<Context> context, LanguageMode language_mode,
      ParseRestriction restriction, int scope_position);

  // Compile a String source within a context.
  static Handle<SharedFunctionInfo> CompileScript(
      Handle<String> source, Handle<Object> script_name, int line_offset,
      int column_offset, bool is_debugger_script, bool is_shared_cross_origin,
      Handle<Object> source_map_url, Handle<Context> context,
      v8::Extension* extension, ScriptData** cached_data,
      ScriptCompiler::CompileOptions compile_options,
      NativesFlag is_natives_code, bool is_module);

  static Handle<SharedFunctionInfo> CompileStreamedScript(Handle<Script> script,
                                                          ParseInfo* info,
                                                          int source_length);

  // Create a shared function info object (the code may be lazily compiled).
  static Handle<SharedFunctionInfo> BuildFunctionInfo(FunctionLiteral* node,
                                                      Handle<Script> script,
                                                      CompilationInfo* outer);

  enum ConcurrencyMode { NOT_CONCURRENT, CONCURRENT };

  // Generate and return optimized code or start a concurrent optimization job.
  // In the latter case, return the InOptimizationQueue builtin.  On failure,
  // return the empty handle.
  MUST_USE_RESULT static MaybeHandle<Code> GetOptimizedCode(
      Handle<JSFunction> function,
      Handle<Code> current_code,
      ConcurrencyMode mode,
      BailoutId osr_ast_id = BailoutId::None());

  // Generate and return code from previously queued optimization job.
  // On failure, return the empty handle.
  static Handle<Code> GetConcurrentlyOptimizedCode(OptimizedCompileJob* job);

  // TODO(titzer): move this method out of the compiler.
  static bool DebuggerWantsEagerCompilation(
      Isolate* isolate, bool allow_lazy_without_ctx = false);
};


class CompilationPhase BASE_EMBEDDED {
 public:
  CompilationPhase(const char* name, CompilationInfo* info);
  ~CompilationPhase();

 protected:
  bool ShouldProduceTraceOutput() const;

  const char* name() const { return name_; }
  CompilationInfo* info() const { return info_; }
  Isolate* isolate() const { return info()->isolate(); }
  Zone* zone() { return &zone_; }

 private:
  const char* name_;
  CompilationInfo* info_;
  Zone zone_;
  size_t info_zone_start_allocation_size_;
  base::ElapsedTimer timer_;

  DISALLOW_COPY_AND_ASSIGN(CompilationPhase);
};

} }  // namespace v8::internal

#endif  // V8_COMPILER_H_
