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

static const int kPrologueOffsetNotSet = -1;

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
  CompilationInfo(Handle<Script> script, Zone* zone);
  CompilationInfo(Handle<SharedFunctionInfo> shared_info, Zone* zone);
  CompilationInfo(Handle<JSFunction> closure, Zone* zone);
  CompilationInfo(HydrogenCodeStub* stub, Isolate* isolate, Zone* zone);

  ~CompilationInfo();

  Isolate* isolate() {
    ASSERT(Isolate::Current() == isolate_);
    return isolate_;
  }
  Zone* zone() { return zone_; }
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
  void MarkCompilingForDebugging(Handle<Code> current_code) {
    ASSERT(mode_ != OPTIMIZE);
    ASSERT(current_code->kind() == Code::FUNCTION);
    flags_ |= IsCompilingForDebugging::encode(true);
    if (current_code->is_compiled_optimizable()) {
      EnableDeoptimizationSupport();
    } else {
      mode_ = CompilationInfo::NONOPT;
    }
  }
  bool IsCompilingForDebugging() {
    return IsCompilingForDebugging::decode(flags_);
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
  void SetOptimizing(BailoutId osr_ast_id) {
    SetMode(OPTIMIZE);
    osr_ast_id_ = osr_ast_id;
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

  // Disable all optimization attempts of this info for the rest of the
  // current compilation pipeline.
  void AbortOptimization();

  void set_deferred_handles(DeferredHandles* deferred_handles) {
    ASSERT(deferred_handles_ == NULL);
    deferred_handles_ = deferred_handles;
  }

  void SaveHandles() {
    SaveHandle(&closure_);
    SaveHandle(&shared_info_);
    SaveHandle(&context_);
    SaveHandle(&script_);
  }

  const char* bailout_reason() const { return bailout_reason_; }
  void set_bailout_reason(const char* reason) { bailout_reason_ = reason; }

  int prologue_offset() const {
    ASSERT_NE(kPrologueOffsetNotSet, prologue_offset_);
    return prologue_offset_;
  }

  void set_prologue_offset(int prologue_offset) {
    ASSERT_EQ(kPrologueOffsetNotSet, prologue_offset_);
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
    ASSERT(V8::UseCrankshaft());
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

  // The zone from which the compilation pipeline working on this
  // CompilationInfo allocates.
  Zone* zone_;

  DeferredHandles* deferred_handles_;

  template<typename T>
  void SaveHandle(Handle<T> *object) {
    if (!object->is_null()) {
      Handle<T> handle(*(*object));
      *object = handle;
    }
  }

  const char* bailout_reason_;

  int prologue_offset_;

  List<OffsetRange>* no_frame_ranges_;

  // A copy of shared_info()->opt_count() to avoid handle deref
  // during graph optimization.
  int opt_count_;

  DISALLOW_COPY_AND_ASSIGN(CompilationInfo);
};


// Exactly like a CompilationInfo, except also creates and enters a
// Zone on construction and deallocates it on exit.
class CompilationInfoWithZone: public CompilationInfo {
 public:
  explicit CompilationInfoWithZone(Handle<Script> script)
      : CompilationInfo(script, &zone_),
        zone_(script->GetIsolate()),
        zone_scope_(&zone_, DELETE_ON_EXIT) {}
  explicit CompilationInfoWithZone(Handle<SharedFunctionInfo> shared_info)
      : CompilationInfo(shared_info, &zone_),
        zone_(shared_info->GetIsolate()),
        zone_scope_(&zone_, DELETE_ON_EXIT) {}
  explicit CompilationInfoWithZone(Handle<JSFunction> closure)
      : CompilationInfo(closure, &zone_),
        zone_(closure->GetIsolate()),
        zone_scope_(&zone_, DELETE_ON_EXIT) {}
  explicit CompilationInfoWithZone(HydrogenCodeStub* stub, Isolate* isolate)
      : CompilationInfo(stub, isolate, &zone_),
        zone_(isolate),
        zone_scope_(&zone_, DELETE_ON_EXIT) {}

 private:
  Zone zone_;
  ZoneScope zone_scope_;
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
class OptimizingCompiler: public ZoneObject {
 public:
  explicit OptimizingCompiler(CompilationInfo* info)
      : info_(info),
        graph_builder_(NULL),
        graph_(NULL),
        chunk_(NULL),
        time_taken_to_create_graph_(0),
        time_taken_to_optimize_(0),
        time_taken_to_codegen_(0),
        last_status_(FAILED) { }

  enum Status {
    FAILED, BAILED_OUT, SUCCEEDED
  };

  MUST_USE_RESULT Status CreateGraph();
  MUST_USE_RESULT Status OptimizeGraph();
  MUST_USE_RESULT Status GenerateAndInstallCode();

  Status last_status() const { return last_status_; }
  CompilationInfo* info() const { return info_; }
  Isolate* isolate() const { return info()->isolate(); }

  MUST_USE_RESULT Status AbortOptimization() {
    info_->AbortOptimization();
    info_->shared_info()->DisableOptimization(info_->bailout_reason());
    return SetLastStatus(BAILED_OUT);
  }

 private:
  CompilationInfo* info_;
  HOptimizedGraphBuilder* graph_builder_;
  HGraph* graph_;
  LChunk* chunk_;
  int64_t time_taken_to_create_graph_;
  int64_t time_taken_to_optimize_;
  int64_t time_taken_to_codegen_;
  Status last_status_;

  MUST_USE_RESULT Status SetLastStatus(Status status) {
    last_status_ = status;
    return last_status_;
  }
  void RecordOptimizationStats();

  struct Timer {
    Timer(OptimizingCompiler* compiler, int64_t* location)
        : compiler_(compiler),
          start_(OS::Ticks()),
          location_(location) { }

    ~Timer() {
      *location_ += (OS::Ticks() - start_);
    }

    OptimizingCompiler* compiler_;
    int64_t start_;
    int64_t* location_;
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
  static const int kMaxInliningLevels = 3;

  // Call count before primitive functions trigger their own optimization.
  static const int kCallsUntilPrimitiveOpt = 200;

  // All routines return a SharedFunctionInfo.
  // If an error occurs an exception is raised and the return handle
  // contains NULL.

  // Compile a String source within a context.
  static Handle<SharedFunctionInfo> Compile(Handle<String> source,
                                            Handle<Object> script_name,
                                            int line_offset,
                                            int column_offset,
                                            Handle<Context> context,
                                            v8::Extension* extension,
                                            ScriptDataImpl* pre_data,
                                            Handle<Object> script_data,
                                            NativesFlag is_natives_code);

  // Compile a String source within a context for Eval.
  static Handle<SharedFunctionInfo> CompileEval(Handle<String> source,
                                                Handle<Context> context,
                                                bool is_global,
                                                LanguageMode language_mode,
                                                ParseRestriction restriction,
                                                int scope_position);

  // Compile from function info (used for lazy compilation). Returns true on
  // success and false if the compilation resulted in a stack overflow.
  static bool CompileLazy(CompilationInfo* info);

  static void RecompileParallel(Handle<JSFunction> function);

  // Compile a shared function info object (the function is possibly lazily
  // compiled).
  static Handle<SharedFunctionInfo> BuildFunctionInfo(FunctionLiteral* node,
                                                      Handle<Script> script);

  // Set the function info for a newly compiled function.
  static void SetFunctionInfo(Handle<SharedFunctionInfo> function_info,
                              FunctionLiteral* lit,
                              bool is_toplevel,
                              Handle<Script> script);

  static void InstallOptimizedCode(OptimizingCompiler* info);

#ifdef ENABLE_DEBUGGER_SUPPORT
  static bool MakeCodeForLiveEdit(CompilationInfo* info);
#endif

  static void RecordFunctionCompilation(Logger::LogEventsAndTags tag,
                                        CompilationInfo* info,
                                        Handle<SharedFunctionInfo> shared);
};


} }  // namespace v8::internal

#endif  // V8_COMPILER_H_
