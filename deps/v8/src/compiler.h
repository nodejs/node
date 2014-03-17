// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_COMPILER_H_
#define V8_COMPILER_H_

#include "allocation.h"
#include "ast.h"
#include "zone.h"

namespace v8 {
namespace internal {

class ScriptDataImpl;
class HydrogenCodeStub;

// ParseRestriction is used to restrict the set of valid statements in a
// unit of compilation.  Restriction violations cause a syntax error.
enum ParseRestriction {
  NO_PARSE_RESTRICTION,         // All expressions are allowed.
  ONLY_SINGLE_FUNCTION_LITERAL  // Only a single FunctionLiteral expression.
};

struct OffsetRange {
  OffsetRange(int from, int to) : from(from), to(to) {}
  int from;
  int to;
};

// CompilationInfo encapsulates some information known at compile time.  It
// is constructed based on the resources available at compile-time.
class CompilationInfo {
 public:
  CompilationInfo(Handle<JSFunction> closure, Zone* zone);
  virtual ~CompilationInfo();

  Isolate* isolate() const {
    return isolate_;
  }
  Zone* zone() { return zone_; }
  bool is_osr() const { return !osr_ast_id_.IsNone(); }
  bool is_lazy() const { return IsLazy::decode(flags_); }
  bool is_eval() const { return IsEval::decode(flags_); }
  bool is_global() const { return IsGlobal::decode(flags_); }
  bool is_classic_mode() const { return language_mode() == CLASSIC_MODE; }
  bool is_extended_mode() const { return language_mode() == EXTENDED_MODE; }
  LanguageMode language_mode() const {
    return LanguageModeField::decode(flags_);
  }
  bool is_in_loop() const { return IsInLoop::decode(flags_); }
  FunctionLiteral* function() const { return function_; }
  Scope* scope() const { return scope_; }
  Scope* global_scope() const { return global_scope_; }
  Handle<Code> code() const { return code_; }
  Handle<JSFunction> closure() const { return closure_; }
  Handle<SharedFunctionInfo> shared_info() const { return shared_info_; }
  Handle<Script> script() const { return script_; }
  HydrogenCodeStub* code_stub() const {return code_stub_; }
  v8::Extension* extension() const { return extension_; }
  ScriptDataImpl* pre_parse_data() const { return pre_parse_data_; }
  Handle<Context> context() const { return context_; }
  BailoutId osr_ast_id() const { return osr_ast_id_; }
  Handle<Code> unoptimized_code() const { return unoptimized_code_; }
  int opt_count() const { return opt_count_; }
  int num_parameters() const;
  int num_heap_slots() const;
  Code::Flags flags() const;

  void MarkAsEval() {
    ASSERT(!is_lazy());
    flags_ |= IsEval::encode(true);
  }
  void MarkAsGlobal() {
    ASSERT(!is_lazy());
    flags_ |= IsGlobal::encode(true);
  }
  void set_parameter_count(int parameter_count) {
    ASSERT(IsStub());
    parameter_count_ = parameter_count;
  }

  void set_this_has_uses(bool has_no_uses) {
    this_has_uses_ = has_no_uses;
  }
  bool this_has_uses() {
    return this_has_uses_;
  }
  void SetLanguageMode(LanguageMode language_mode) {
    ASSERT(this->language_mode() == CLASSIC_MODE ||
           this->language_mode() == language_mode ||
           language_mode == EXTENDED_MODE);
    flags_ = LanguageModeField::update(flags_, language_mode);
  }
  void MarkAsInLoop() {
    ASSERT(is_lazy());
    flags_ |= IsInLoop::encode(true);
  }
  void MarkAsNative() {
    flags_ |= IsNative::encode(true);
  }

  bool is_native() const {
    return IsNative::decode(flags_);
  }

  bool is_calling() const {
    return is_deferred_calling() || is_non_deferred_calling();
  }

  void MarkAsDeferredCalling() {
    flags_ |= IsDeferredCalling::encode(true);
  }

  bool is_deferred_calling() const {
    return IsDeferredCalling::decode(flags_);
  }

  void MarkAsNonDeferredCalling() {
    flags_ |= IsNonDeferredCalling::encode(true);
  }

  bool is_non_deferred_calling() const {
    return IsNonDeferredCalling::decode(flags_);
  }

  void MarkAsSavesCallerDoubles() {
    flags_ |= SavesCallerDoubles::encode(true);
  }

  bool saves_caller_doubles() const {
    return SavesCallerDoubles::decode(flags_);
  }

  void MarkAsRequiresFrame() {
    flags_ |= RequiresFrame::encode(true);
  }

  bool requires_frame() const {
    return RequiresFrame::decode(flags_);
  }

  void SetParseRestriction(ParseRestriction restriction) {
    flags_ = ParseRestricitonField::update(flags_, restriction);
  }

  ParseRestriction parse_restriction() const {
    return ParseRestricitonField::decode(flags_);
  }

  void SetFunction(FunctionLiteral* literal) {
    ASSERT(function_ == NULL);
    function_ = literal;
  }
  void SetScope(Scope* scope) {
    ASSERT(scope_ == NULL);
    scope_ = scope;
  }
  void SetGlobalScope(Scope* global_scope) {
    ASSERT(global_scope_ == NULL);
    global_scope_ = global_scope;
  }
  void SetCode(Handle<Code> code) { code_ = code; }
  void SetExtension(v8::Extension* extension) {
    ASSERT(!is_lazy());
    extension_ = extension;
  }
  void SetPreParseData(ScriptDataImpl* pre_parse_data) {
    ASSERT(!is_lazy());
    pre_parse_data_ = pre_parse_data;
  }
  void SetContext(Handle<Context> context) {
    context_ = context;
  }

  void MarkCompilingForDebugging() {
    flags_ |= IsCompilingForDebugging::encode(true);
  }
  bool IsCompilingForDebugging() {
    return IsCompilingForDebugging::decode(flags_);
  }
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
    ASSERT(!shared_info_.is_null());
    SetMode(OPTIMIZE);
    osr_ast_id_ = osr_ast_id;
    unoptimized_code_ = unoptimized;
  }
  void DisableOptimization();

  // Deoptimization support.
  bool HasDeoptimizationSupport() const {
    return SupportsDeoptimization::decode(flags_);
  }
  void EnableDeoptimizationSupport() {
    ASSERT(IsOptimizable());
    flags_ |= SupportsDeoptimization::encode(true);
  }

  // Determines whether or not to insert a self-optimization header.
  bool ShouldSelfOptimize();

  void set_deferred_handles(DeferredHandles* deferred_handles) {
    ASSERT(deferred_handles_ == NULL);
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

  void SaveHandles() {
    SaveHandle(&closure_);
    SaveHandle(&shared_info_);
    SaveHandle(&context_);
    SaveHandle(&script_);
    SaveHandle(&unoptimized_code_);
  }

  BailoutReason bailout_reason() const { return bailout_reason_; }
  void set_bailout_reason(BailoutReason reason) { bailout_reason_ = reason; }

  int prologue_offset() const {
    ASSERT_NE(Code::kPrologueOffsetNotSet, prologue_offset_);
    return prologue_offset_;
  }

  void set_prologue_offset(int prologue_offset) {
    ASSERT_EQ(Code::kPrologueOffsetNotSet, prologue_offset_);
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

  Handle<Foreign> object_wrapper() {
    if (object_wrapper_.is_null()) {
      object_wrapper_ =
          isolate()->factory()->NewForeign(reinterpret_cast<Address>(this));
    }
    return object_wrapper_;
  }

  void AbortDueToDependencyChange() {
    ASSERT(!OptimizingCompilerThread::IsOptimizerThread(isolate()));
    abort_due_to_dependency_ = true;
  }

  bool HasAbortedDueToDependencyChange() {
    ASSERT(!OptimizingCompilerThread::IsOptimizerThread(isolate()));
    return abort_due_to_dependency_;
  }

  bool HasSameOsrEntry(Handle<JSFunction> function, BailoutId osr_ast_id) {
    return osr_ast_id_ == osr_ast_id && function.is_identical_to(closure_);
  }

 protected:
  CompilationInfo(Handle<Script> script,
                  Zone* zone);
  CompilationInfo(Handle<SharedFunctionInfo> shared_info,
                  Zone* zone);
  CompilationInfo(HydrogenCodeStub* stub,
                  Isolate* isolate,
                  Zone* zone);

 private:
  Isolate* isolate_;

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

  void Initialize(Isolate* isolate, Mode mode, Zone* zone);

  void SetMode(Mode mode) {
    ASSERT(isolate()->use_crankshaft());
    mode_ = mode;
  }

  // Flags using template class BitField<type, start, length>.  All are
  // false by default.
  //
  // Compilation is either eager or lazy.
  class IsLazy:   public BitField<bool, 0, 1> {};
  // Flags that can be set for eager compilation.
  class IsEval:   public BitField<bool, 1, 1> {};
  class IsGlobal: public BitField<bool, 2, 1> {};
  // Flags that can be set for lazy compilation.
  class IsInLoop: public BitField<bool, 3, 1> {};
  // Strict mode - used in eager compilation.
  class LanguageModeField: public BitField<LanguageMode, 4, 2> {};
  // Is this a function from our natives.
  class IsNative: public BitField<bool, 6, 1> {};
  // Is this code being compiled with support for deoptimization..
  class SupportsDeoptimization: public BitField<bool, 7, 1> {};
  // If compiling for debugging produce just full code matching the
  // initial mode setting.
  class IsCompilingForDebugging: public BitField<bool, 8, 1> {};
  // If the compiled code contains calls that require building a frame
  class IsCalling: public BitField<bool, 9, 1> {};
  // If the compiled code contains calls that require building a frame
  class IsDeferredCalling: public BitField<bool, 10, 1> {};
  // If the compiled code contains calls that require building a frame
  class IsNonDeferredCalling: public BitField<bool, 11, 1> {};
  // If the compiled code saves double caller registers that it clobbers.
  class SavesCallerDoubles: public BitField<bool, 12, 1> {};
  // If the set of valid statements is restricted.
  class ParseRestricitonField: public BitField<ParseRestriction, 13, 1> {};
  // If the function requires a frame (for unspecified reasons)
  class RequiresFrame: public BitField<bool, 14, 1> {};

  unsigned flags_;

  // Fields filled in by the compilation pipeline.
  // AST filled in by the parser.
  FunctionLiteral* function_;
  // The scope of the function literal as a convenience.  Set to indicate
  // that scopes have been analyzed.
  Scope* scope_;
  // The global scope provided as a convenience.
  Scope* global_scope_;
  // For compiled stubs, the stub object
  HydrogenCodeStub* code_stub_;
  // The compiled code.
  Handle<Code> code_;

  // Possible initial inputs to the compilation process.
  Handle<JSFunction> closure_;
  Handle<SharedFunctionInfo> shared_info_;
  Handle<Script> script_;

  // Fields possibly needed for eager compilation, NULL by default.
  v8::Extension* extension_;
  ScriptDataImpl* pre_parse_data_;

  // The context of the caller for eval code, and the global context for a
  // global script. Will be a null handle otherwise.
  Handle<Context> context_;

  // Compilation mode flag and whether deoptimization is allowed.
  Mode mode_;
  BailoutId osr_ast_id_;
  // The unoptimized code we patched for OSR may not be the shared code
  // afterwards, since we may need to compile it again to include deoptimization
  // data.  Keep track which code we patched.
  Handle<Code> unoptimized_code_;

  // Flag whether compilation needs to be aborted due to dependency change.
  bool abort_due_to_dependency_;

  // The zone from which the compilation pipeline working on this
  // CompilationInfo allocates.
  Zone* zone_;

  DeferredHandles* deferred_handles_;

  ZoneList<Handle<HeapObject> >* dependencies_[DependentCode::kGroupCount];

  template<typename T>
  void SaveHandle(Handle<T> *object) {
    if (!object->is_null()) {
      Handle<T> handle(*(*object));
      *object = handle;
    }
  }

  BailoutReason bailout_reason_;

  int prologue_offset_;

  List<OffsetRange>* no_frame_ranges_;

  // A copy of shared_info()->opt_count() to avoid handle deref
  // during graph optimization.
  int opt_count_;

  // Number of parameters used for compilation of stubs that require arguments.
  int parameter_count_;

  bool this_has_uses_;

  Handle<Foreign> object_wrapper_;

  DISALLOW_COPY_AND_ASSIGN(CompilationInfo);
};


// Exactly like a CompilationInfo, except also creates and enters a
// Zone on construction and deallocates it on exit.
class CompilationInfoWithZone: public CompilationInfo {
 public:
  explicit CompilationInfoWithZone(Handle<Script> script)
      : CompilationInfo(script, &zone_),
        zone_(script->GetIsolate()) {}
  explicit CompilationInfoWithZone(Handle<SharedFunctionInfo> shared_info)
      : CompilationInfo(shared_info, &zone_),
        zone_(shared_info->GetIsolate()) {}
  explicit CompilationInfoWithZone(Handle<JSFunction> closure)
      : CompilationInfo(closure, &zone_),
        zone_(closure->GetIsolate()) {}
  CompilationInfoWithZone(HydrogenCodeStub* stub, Isolate* isolate)
      : CompilationInfo(stub, isolate, &zone_),
        zone_(isolate) {}

  // Virtual destructor because a CompilationInfoWithZone has to exit the
  // zone scope and get rid of dependent maps even when the destructor is
  // called when cast as a CompilationInfo.
  virtual ~CompilationInfoWithZone() {
    RollbackDependencies();
  }

 private:
  Zone zone_;
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

  MUST_USE_RESULT Status AbortOptimization(
      BailoutReason reason = kNoReason) {
    if (reason != kNoReason) info_->set_bailout_reason(reason);
    return SetLastStatus(BAILED_OUT);
  }

  MUST_USE_RESULT Status AbortAndDisableOptimization(
      BailoutReason reason = kNoReason) {
    if (reason != kNoReason) info_->set_bailout_reason(reason);
    info_->shared_info()->DisableOptimization(info_->bailout_reason());
    return SetLastStatus(BAILED_OUT);
  }

  void WaitForInstall() {
    ASSERT(info_->is_osr());
    awaiting_install_ = true;
  }

  bool IsWaitingForInstall() { return awaiting_install_; }

 private:
  CompilationInfo* info_;
  HOptimizedGraphBuilder* graph_builder_;
  HGraph* graph_;
  LChunk* chunk_;
  TimeDelta time_taken_to_create_graph_;
  TimeDelta time_taken_to_optimize_;
  TimeDelta time_taken_to_codegen_;
  Status last_status_;
  bool awaiting_install_;

  MUST_USE_RESULT Status SetLastStatus(Status status) {
    last_status_ = status;
    return last_status_;
  }
  void RecordOptimizationStats();

  struct Timer {
    Timer(OptimizedCompileJob* job, TimeDelta* location)
        : job_(job), location_(location) {
      ASSERT(location_ != NULL);
      timer_.Start();
    }

    ~Timer() {
      *location_ += timer_.Elapsed();
    }

    OptimizedCompileJob* job_;
    ElapsedTimer timer_;
    TimeDelta* location_;
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
  static Handle<Code> GetUnoptimizedCode(Handle<JSFunction> function);
  static Handle<Code> GetUnoptimizedCode(Handle<SharedFunctionInfo> shared);
  static bool EnsureCompiled(Handle<JSFunction> function,
                             ClearExceptionFlag flag);
  static Handle<Code> GetCodeForDebugging(Handle<JSFunction> function);

#ifdef ENABLE_DEBUGGER_SUPPORT
  static void CompileForLiveEdit(Handle<Script> script);
#endif

  // Compile a String source within a context for eval.
  static Handle<JSFunction> GetFunctionFromEval(Handle<String> source,
                                                Handle<Context> context,
                                                LanguageMode language_mode,
                                                ParseRestriction restriction,
                                                int scope_position);

  // Compile a String source within a context.
  static Handle<SharedFunctionInfo> CompileScript(Handle<String> source,
                                                  Handle<Object> script_name,
                                                  int line_offset,
                                                  int column_offset,
                                                  bool is_shared_cross_origin,
                                                  Handle<Context> context,
                                                  v8::Extension* extension,
                                                  ScriptDataImpl* pre_data,
                                                  Handle<Object> script_data,
                                                  NativesFlag is_natives_code);

  // Create a shared function info object (the code may be lazily compiled).
  static Handle<SharedFunctionInfo> BuildFunctionInfo(FunctionLiteral* node,
                                                      Handle<Script> script);

  enum ConcurrencyMode { NOT_CONCURRENT, CONCURRENT };

  // Generate and return optimized code or start a concurrent optimization job.
  // In the latter case, return the InOptimizationQueue builtin.  On failure,
  // return the empty handle.
  static Handle<Code> GetOptimizedCode(
      Handle<JSFunction> function,
      Handle<Code> current_code,
      ConcurrencyMode mode,
      BailoutId osr_ast_id = BailoutId::None());

  // Generate and return code from previously queued optimization job.
  // On failure, return the empty handle.
  static Handle<Code> GetConcurrentlyOptimizedCode(OptimizedCompileJob* job);

  static void RecordFunctionCompilation(Logger::LogEventsAndTags tag,
                                        CompilationInfo* info,
                                        Handle<SharedFunctionInfo> shared);
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
  unsigned info_zone_start_allocation_size_;
  ElapsedTimer timer_;

  DISALLOW_COPY_AND_ASSIGN(CompilationPhase);
};


} }  // namespace v8::internal

#endif  // V8_COMPILER_H_
