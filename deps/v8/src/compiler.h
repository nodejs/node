// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_H_
#define V8_COMPILER_H_

#include <memory>

#include "src/allocation.h"
#include "src/bailout-reason.h"
#include "src/compilation-dependencies.h"
#include "src/contexts.h"
#include "src/frames.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/source-position-table.h"
#include "src/source-position.h"
#include "src/zone.h"

namespace v8 {
namespace internal {

// Forward declarations.
class CompilationInfo;
class CompilationJob;
class JavaScriptFrame;
class ParseInfo;
class ScriptData;

// The V8 compiler API.
//
// This is the central hub for dispatching to the various compilers within V8.
// Logic for which compiler to choose and how to wire compilation results into
// the object heap should be kept inside this class.
//
// General strategy: Scripts are translated into anonymous functions w/o
// parameters which then can be executed. If the source code contains other
// functions, they might be compiled and allocated as part of the compilation
// of the source code or deferred for lazy compilation at a later point.
class Compiler : public AllStatic {
 public:
  enum ClearExceptionFlag { KEEP_EXCEPTION, CLEAR_EXCEPTION };
  enum ConcurrencyMode { NOT_CONCURRENT, CONCURRENT };
  enum CompilationTier { INTERPRETED, BASELINE, OPTIMIZED };

  // ===========================================================================
  // The following family of methods ensures a given function is compiled. The
  // general contract is that failures will be reported by returning {false},
  // whereas successful compilation ensures the {is_compiled} predicate on the
  // given function holds (except for live-edit, which compiles the world).

  static bool Compile(Handle<JSFunction> function, ClearExceptionFlag flag);
  static bool CompileBaseline(Handle<JSFunction> function);
  static bool CompileOptimized(Handle<JSFunction> function, ConcurrencyMode);
  static bool CompileDebugCode(Handle<JSFunction> function);
  static bool CompileDebugCode(Handle<SharedFunctionInfo> shared);
  static MaybeHandle<JSArray> CompileForLiveEdit(Handle<Script> script);

  // Generate and install code from previously queued compilation job.
  static void FinalizeCompilationJob(CompilationJob* job);

  // Give the compiler a chance to perform low-latency initialization tasks of
  // the given {function} on its instantiation. Note that only the runtime will
  // offer this chance, optimized closure instantiation will not call this.
  static void PostInstantiation(Handle<JSFunction> function, PretenureFlag);

  // Parser::Parse, then Compiler::Analyze.
  static bool ParseAndAnalyze(ParseInfo* info);
  // Rewrite, analyze scopes, and renumber.
  static bool Analyze(ParseInfo* info);
  // Adds deoptimization support, requires ParseAndAnalyze.
  static bool EnsureDeoptimizationSupport(CompilationInfo* info);
  // Ensures that bytecode is generated, calls ParseAndAnalyze internally.
  static bool EnsureBytecode(CompilationInfo* info);

  // The next compilation tier which the function should  be compiled to for
  // optimization. This is used as a hint by the runtime profiler.
  static CompilationTier NextCompilationTier(JSFunction* function);

  // ===========================================================================
  // The following family of methods instantiates new functions for scripts or
  // function literals. The decision whether those functions will be compiled,
  // is left to the discretion of the compiler.
  //
  // Please note this interface returns shared function infos.  This means you
  // need to call Factory::NewFunctionFromSharedFunctionInfo before you have a
  // real function with a context.

  // Create a (bound) function for a String source within a context for eval.
  MUST_USE_RESULT static MaybeHandle<JSFunction> GetFunctionFromEval(
      Handle<String> source, Handle<SharedFunctionInfo> outer_info,
      Handle<Context> context, LanguageMode language_mode,
      ParseRestriction restriction, int eval_scope_position, int eval_position,
      int line_offset = 0, int column_offset = 0,
      Handle<Object> script_name = Handle<Object>(),
      ScriptOriginOptions options = ScriptOriginOptions());

  // Create a (bound) function for a String source within a context for eval.
  MUST_USE_RESULT static MaybeHandle<JSFunction> GetFunctionFromString(
      Handle<Context> context, Handle<String> source,
      ParseRestriction restriction);

  // Create a shared function info object for a String source within a context.
  static Handle<SharedFunctionInfo> GetSharedFunctionInfoForScript(
      Handle<String> source, Handle<Object> script_name, int line_offset,
      int column_offset, ScriptOriginOptions resource_options,
      Handle<Object> source_map_url, Handle<Context> context,
      v8::Extension* extension, ScriptData** cached_data,
      ScriptCompiler::CompileOptions compile_options,
      NativesFlag is_natives_code, bool is_module);

  // Create a shared function info object for a Script that has already been
  // parsed while the script was being loaded from a streamed source.
  static Handle<SharedFunctionInfo> GetSharedFunctionInfoForStreamedScript(
      Handle<Script> script, ParseInfo* info, int source_length);

  // Create a shared function info object (the code may be lazily compiled).
  static Handle<SharedFunctionInfo> GetSharedFunctionInfo(
      FunctionLiteral* node, Handle<Script> script, CompilationInfo* outer);

  // Create a shared function info object for a native function literal.
  static Handle<SharedFunctionInfo> GetSharedFunctionInfoForNative(
      v8::Extension* extension, Handle<String> name);

  // ===========================================================================
  // The following family of methods provides support for OSR. Code generated
  // for entry via OSR might not be suitable for normal entry, hence will be
  // returned directly to the caller.
  //
  // Please note this interface is the only part dealing with {Code} objects
  // directly. Other methods are agnostic to {Code} and can use an interpreter
  // instead of generating JIT code for a function at all.

  // Generate and return optimized code for OSR, or empty handle on failure.
  MUST_USE_RESULT static MaybeHandle<Code> GetOptimizedCodeForOSR(
      Handle<JSFunction> function, BailoutId osr_ast_id,
      JavaScriptFrame* osr_frame);
};


// CompilationInfo encapsulates some information known at compile time.  It
// is constructed based on the resources available at compile-time.
class CompilationInfo final {
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
    kSerializing = 1 << 7,
    kFunctionContextSpecializing = 1 << 8,
    kFrameSpecializing = 1 << 9,
    kNativeContextSpecializing = 1 << 10,
    kInliningEnabled = 1 << 11,
    kDisableFutureOptimization = 1 << 12,
    kSplittingEnabled = 1 << 13,
    kDeoptimizationEnabled = 1 << 14,
    kSourcePositionsEnabled = 1 << 15,
    kBailoutOnUninitialized = 1 << 16,
    kOptimizeFromBytecode = 1 << 17,
    kTypeFeedbackEnabled = 1 << 18,
    kAccessorInliningEnabled = 1 << 19,
  };

  CompilationInfo(ParseInfo* parse_info, Handle<JSFunction> closure);
  CompilationInfo(Vector<const char> debug_name, Isolate* isolate, Zone* zone,
                  Code::Flags code_flags = Code::ComputeFlags(Code::STUB));
  ~CompilationInfo();

  ParseInfo* parse_info() const { return parse_info_; }

  // -----------------------------------------------------------
  // TODO(titzer): inline and delete accessors of ParseInfo
  // -----------------------------------------------------------
  Handle<Script> script() const;
  FunctionLiteral* literal() const;
  DeclarationScope* scope() const;
  Handle<Context> context() const;
  Handle<SharedFunctionInfo> shared_info() const;
  bool has_shared_info() const;
  // -----------------------------------------------------------

  Isolate* isolate() const {
    return isolate_;
  }
  Zone* zone() { return zone_; }
  bool is_osr() const { return !osr_ast_id_.IsNone(); }
  Handle<JSFunction> closure() const { return closure_; }
  Handle<Code> code() const { return code_; }
  Code::Flags code_flags() const { return code_flags_; }
  BailoutId osr_ast_id() const { return osr_ast_id_; }
  JavaScriptFrame* osr_frame() const { return osr_frame_; }
  int num_parameters() const;
  int num_parameters_including_this() const;
  bool is_this_defined() const;

  void set_parameter_count(int parameter_count) {
    DCHECK(IsStub());
    parameter_count_ = parameter_count;
  }

  bool has_bytecode_array() const { return !bytecode_array_.is_null(); }
  Handle<BytecodeArray> bytecode_array() const { return bytecode_array_; }

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

  // Compiles marked as debug produce unoptimized code with debug break slots.
  // Inner functions that cannot be compiled w/o context are compiled eagerly.
  // Always include deoptimization support to avoid having to recompile again.
  void MarkAsDebug() {
    SetFlag(kDebug);
    SetFlag(kDeoptimizationSupport);
  }

  bool is_debug() const { return GetFlag(kDebug); }

  void PrepareForSerializing() { SetFlag(kSerializing); }

  bool will_serialize() const { return GetFlag(kSerializing); }

  void MarkAsFunctionContextSpecializing() {
    SetFlag(kFunctionContextSpecializing);
  }

  bool is_function_context_specializing() const {
    return GetFlag(kFunctionContextSpecializing);
  }

  void MarkAsFrameSpecializing() { SetFlag(kFrameSpecializing); }

  bool is_frame_specializing() const { return GetFlag(kFrameSpecializing); }

  void MarkAsNativeContextSpecializing() {
    SetFlag(kNativeContextSpecializing);
  }

  bool is_native_context_specializing() const {
    return GetFlag(kNativeContextSpecializing);
  }

  void MarkAsDeoptimizationEnabled() { SetFlag(kDeoptimizationEnabled); }

  bool is_deoptimization_enabled() const {
    return GetFlag(kDeoptimizationEnabled);
  }

  void MarkAsTypeFeedbackEnabled() { SetFlag(kTypeFeedbackEnabled); }

  bool is_type_feedback_enabled() const {
    return GetFlag(kTypeFeedbackEnabled);
  }

  void MarkAsAccessorInliningEnabled() { SetFlag(kAccessorInliningEnabled); }

  bool is_accessor_inlining_enabled() const {
    return GetFlag(kAccessorInliningEnabled);
  }

  void MarkAsSourcePositionsEnabled() { SetFlag(kSourcePositionsEnabled); }

  bool is_source_positions_enabled() const {
    return GetFlag(kSourcePositionsEnabled);
  }

  void MarkAsInliningEnabled() { SetFlag(kInliningEnabled); }

  bool is_inlining_enabled() const { return GetFlag(kInliningEnabled); }

  void MarkAsSplittingEnabled() { SetFlag(kSplittingEnabled); }

  bool is_splitting_enabled() const { return GetFlag(kSplittingEnabled); }

  void MarkAsBailoutOnUninitialized() { SetFlag(kBailoutOnUninitialized); }

  bool is_bailout_on_uninitialized() const {
    return GetFlag(kBailoutOnUninitialized);
  }

  void MarkAsOptimizeFromBytecode() { SetFlag(kOptimizeFromBytecode); }

  bool is_optimizing_from_bytecode() const {
    return GetFlag(kOptimizeFromBytecode);
  }

  bool GeneratePreagedPrologue() const {
    // Generate a pre-aged prologue if we are optimizing for size, which
    // will make code flushing more aggressive. Only apply to Code::FUNCTION,
    // since StaticMarkingVisitor::IsFlushable only flushes proper functions.
    return FLAG_optimize_for_size && FLAG_age_code && !is_debug() &&
           output_code_kind() == Code::FUNCTION;
  }

  void SetCode(Handle<Code> code) { code_ = code; }

  void SetBytecodeArray(Handle<BytecodeArray> bytecode_array) {
    bytecode_array_ = bytecode_array;
  }

  bool ShouldTrapOnDeopt() const {
    return (FLAG_trap_on_deopt && IsOptimizing()) ||
        (FLAG_trap_on_stub_deopt && IsStub());
  }

  bool has_native_context() const {
    return !closure().is_null() && (closure()->native_context() != nullptr);
  }

  Context* native_context() const {
    return has_native_context() ? closure()->native_context() : nullptr;
  }

  bool has_global_object() const { return has_native_context(); }

  JSGlobalObject* global_object() const {
    return has_global_object() ? native_context()->global_object() : nullptr;
  }

  // Accessors for the different compilation modes.
  bool IsOptimizing() const { return mode_ == OPTIMIZE; }
  bool IsStub() const { return mode_ == STUB; }
  void SetOptimizing() {
    DCHECK(has_shared_info());
    SetMode(OPTIMIZE);
    optimization_id_ = isolate()->NextOptimizationId();
    code_flags_ =
        Code::KindField::update(code_flags_, Code::OPTIMIZED_FUNCTION);
  }
  void SetOptimizingForOsr(BailoutId osr_ast_id, JavaScriptFrame* osr_frame) {
    SetOptimizing();
    osr_ast_id_ = osr_ast_id;
    osr_frame_ = osr_frame;
  }

  // Deoptimization support.
  bool HasDeoptimizationSupport() const {
    return GetFlag(kDeoptimizationSupport);
  }
  void EnableDeoptimizationSupport() {
    DCHECK_EQ(BASE, mode_);
    SetFlag(kDeoptimizationSupport);
  }
  bool ShouldEnsureSpaceForLazyDeopt() { return !IsStub(); }

  bool ExpectsJSReceiverAsReceiver();

  // Determines whether or not to insert a self-optimization header.
  bool ShouldSelfOptimize();

  void set_deferred_handles(DeferredHandles* deferred_handles) {
    DCHECK(deferred_handles_ == NULL);
    deferred_handles_ = deferred_handles;
  }

  void ReopenHandlesInNewHandleScope() {
    closure_ = Handle<JSFunction>(*closure_);
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

  CompilationDependencies* dependencies() { return &dependencies_; }

  int optimization_id() const { return optimization_id_; }

  int osr_expr_stack_height() { return osr_expr_stack_height_; }
  void set_osr_expr_stack_height(int height) {
    DCHECK(height >= 0);
    osr_expr_stack_height_ = height;
  }

  bool has_simple_parameters();

  struct InlinedFunctionHolder {
    Handle<SharedFunctionInfo> shared_info;

    // Root that holds the unoptimized code of the inlined function alive
    // (and out of reach of code flushing) until we finish compilation.
    // Do not remove.
    Handle<Code> inlined_code_object_root;

    explicit InlinedFunctionHolder(
        Handle<SharedFunctionInfo> inlined_shared_info)
        : shared_info(inlined_shared_info),
          inlined_code_object_root(inlined_shared_info->code()) {}
  };

  typedef std::vector<InlinedFunctionHolder> InlinedFunctionList;
  InlinedFunctionList const& inlined_functions() const {
    return inlined_functions_;
  }

  void AddInlinedFunction(Handle<SharedFunctionInfo> inlined_function) {
    inlined_functions_.push_back(InlinedFunctionHolder(inlined_function));
  }

  std::unique_ptr<char[]> GetDebugName() const;

  Code::Kind output_code_kind() const {
    return Code::ExtractKindFromFlags(code_flags_);
  }

  StackFrame::Type GetOutputStackFrameType() const;

  int GetDeclareGlobalsFlags() const;

  SourcePositionTableBuilder::RecordingMode SourcePositionRecordingMode() const;

 private:
  // Compilation mode.
  // BASE is generated by the full codegen, optionally prepared for bailouts.
  // OPTIMIZE is optimized code generated by the Hydrogen-based backend.
  enum Mode {
    BASE,
    OPTIMIZE,
    STUB
  };

  CompilationInfo(ParseInfo* parse_info, Vector<const char> debug_name,
                  Code::Flags code_flags, Mode mode, Isolate* isolate,
                  Zone* zone);

  ParseInfo* parse_info_;
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

  Code::Flags code_flags_;

  Handle<JSFunction> closure_;

  // The compiled code.
  Handle<Code> code_;

  // Compilation mode flag and whether deoptimization is allowed.
  Mode mode_;
  BailoutId osr_ast_id_;

  // Holds the bytecode array generated by the interpreter.
  // TODO(rmcilroy/mstarzinger): Temporary work-around until compiler.cc is
  // refactored to avoid us needing to carry the BytcodeArray around.
  Handle<BytecodeArray> bytecode_array_;

  // The zone from which the compilation pipeline working on this
  // CompilationInfo allocates.
  Zone* zone_;

  DeferredHandles* deferred_handles_;

  // Dependencies for this compilation, e.g. stable maps.
  CompilationDependencies dependencies_;

  BailoutReason bailout_reason_;

  int prologue_offset_;

  bool track_positions_;

  InlinedFunctionList inlined_functions_;

  // Number of parameters used for compilation of stubs that require arguments.
  int parameter_count_;

  int optimization_id_;

  int osr_expr_stack_height_;

  // The current OSR frame for specialization or {nullptr}.
  JavaScriptFrame* osr_frame_ = nullptr;

  Vector<const char> debug_name_;

  DISALLOW_COPY_AND_ASSIGN(CompilationInfo);
};

// A base class for compilation jobs intended to run concurrent to the main
// thread. The job is split into three phases which are called in sequence on
// different threads and with different limitations:
//  1) PrepareJob:   Runs on main thread. No major limitations.
//  2) ExecuteJob:   Runs concurrently. No heap allocation or handle derefs.
//  3) FinalizeJob:  Runs on main thread. No dependency changes.
//
// Each of the three phases can either fail or succeed. The current state of
// the job can be checked using {state()}.
class CompilationJob {
 public:
  enum Status { SUCCEEDED, FAILED };
  enum class State {
    kReadyToPrepare,
    kReadyToExecute,
    kReadyToFinalize,
    kSucceeded,
    kFailed,
  };

  explicit CompilationJob(CompilationInfo* info, const char* compiler_name,
                          State initial_state = State::kReadyToPrepare)
      : info_(info), compiler_name_(compiler_name), state_(initial_state) {}
  virtual ~CompilationJob() {}

  // Prepare the compile job. Must be called on the main thread.
  MUST_USE_RESULT Status PrepareJob();

  // Executes the compile job. Can be called off the main thread.
  MUST_USE_RESULT Status ExecuteJob();

  // Finalizes the compile job. Must be called on the main thread.
  MUST_USE_RESULT Status FinalizeJob();

  // Report a transient failure, try again next time. Should only be called on
  // optimization compilation jobs.
  Status RetryOptimization(BailoutReason reason) {
    DCHECK(info_->IsOptimizing());
    info_->RetryOptimization(reason);
    state_ = State::kFailed;
    return FAILED;
  }

  // Report a persistent failure, disable future optimization on the function.
  // Should only be called on optimization compilation jobs.
  Status AbortOptimization(BailoutReason reason) {
    DCHECK(info_->IsOptimizing());
    info_->AbortOptimization(reason);
    state_ = State::kFailed;
    return FAILED;
  }

  void RecordOptimizationStats();

  State state() const { return state_; }
  CompilationInfo* info() const { return info_; }
  Isolate* isolate() const { return info()->isolate(); }

 protected:
  // Overridden by the actual implementation.
  virtual Status PrepareJobImpl() = 0;
  virtual Status ExecuteJobImpl() = 0;
  virtual Status FinalizeJobImpl() = 0;

  // Registers weak object to optimized code dependencies.
  // TODO(turbofan): Move this to pipeline.cc once Crankshaft dies.
  void RegisterWeakObjectsInOptimizedCode(Handle<Code> code);

 private:
  CompilationInfo* info_;
  base::TimeDelta time_taken_to_prepare_;
  base::TimeDelta time_taken_to_execute_;
  base::TimeDelta time_taken_to_finalize_;
  const char* compiler_name_;
  State state_;

  MUST_USE_RESULT Status UpdateState(Status status, State next_state) {
    if (status == SUCCEEDED) {
      state_ = next_state;
    } else {
      state_ = State::kFailed;
    }
    return status;
  }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_H_
