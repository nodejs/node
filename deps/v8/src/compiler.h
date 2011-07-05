// Copyright 2010 the V8 project authors. All rights reserved.
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

#include "ast.h"
#include "frame-element.h"
#include "register-allocator.h"
#include "zone.h"

namespace v8 {
namespace internal {

class ScriptDataImpl;

// CompilationInfo encapsulates some information known at compile time.  It
// is constructed based on the resources available at compile-time.
class CompilationInfo BASE_EMBEDDED {
 public:
  explicit CompilationInfo(Handle<Script> script);
  explicit CompilationInfo(Handle<SharedFunctionInfo> shared_info);
  explicit CompilationInfo(Handle<JSFunction> closure);

  bool is_lazy() const { return (flags_ & IsLazy::mask()) != 0; }
  bool is_eval() const { return (flags_ & IsEval::mask()) != 0; }
  bool is_global() const { return (flags_ & IsGlobal::mask()) != 0; }
  bool is_strict() const { return (flags_ & IsStrict::mask()) != 0; }
  bool is_in_loop() const { return (flags_ & IsInLoop::mask()) != 0; }
  FunctionLiteral* function() const { return function_; }
  Scope* scope() const { return scope_; }
  Handle<Code> code() const { return code_; }
  Handle<JSFunction> closure() const { return closure_; }
  Handle<SharedFunctionInfo> shared_info() const { return shared_info_; }
  Handle<Script> script() const { return script_; }
  v8::Extension* extension() const { return extension_; }
  ScriptDataImpl* pre_parse_data() const { return pre_parse_data_; }
  Handle<Context> calling_context() const { return calling_context_; }
  int osr_ast_id() const { return osr_ast_id_; }

  void MarkAsEval() {
    ASSERT(!is_lazy());
    flags_ |= IsEval::encode(true);
  }
  void MarkAsGlobal() {
    ASSERT(!is_lazy());
    flags_ |= IsGlobal::encode(true);
  }
  void MarkAsStrict() {
    flags_ |= IsStrict::encode(true);
  }
  StrictModeFlag StrictMode() {
    return is_strict() ? kStrictMode : kNonStrictMode;
  }
  void MarkAsInLoop() {
    ASSERT(is_lazy());
    flags_ |= IsInLoop::encode(true);
  }
  void SetFunction(FunctionLiteral* literal) {
    ASSERT(function_ == NULL);
    function_ = literal;
  }
  void SetScope(Scope* scope) {
    ASSERT(scope_ == NULL);
    scope_ = scope;
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
  void SetCallingContext(Handle<Context> context) {
    ASSERT(is_eval());
    calling_context_ = context;
  }
  void SetOsrAstId(int osr_ast_id) {
    ASSERT(IsOptimizing());
    osr_ast_id_ = osr_ast_id;
  }

  bool has_global_object() const {
    return !closure().is_null() && (closure()->context()->global() != NULL);
  }

  GlobalObject* global_object() const {
    return has_global_object() ? closure()->context()->global() : NULL;
  }

  // Accessors for the different compilation modes.
  bool IsOptimizing() const { return mode_ == OPTIMIZE; }
  bool IsOptimizable() const { return mode_ == BASE; }
  void SetOptimizing(int osr_ast_id) {
    SetMode(OPTIMIZE);
    osr_ast_id_ = osr_ast_id;
  }
  void DisableOptimization();

  // Deoptimization support.
  bool HasDeoptimizationSupport() const { return supports_deoptimization_; }
  void EnableDeoptimizationSupport() {
    ASSERT(IsOptimizable());
    supports_deoptimization_ = true;
  }

  // Determine whether or not we can adaptively optimize.
  bool AllowOptimize() {
    return V8::UseCrankshaft() && !closure_.is_null();
  }

 private:
  // Compilation mode.
  // BASE is generated by the full codegen, optionally prepared for bailouts.
  // OPTIMIZE is optimized code generated by the Hydrogen-based backend.
  // NONOPT is generated by the full codegen or the classic backend
  //   and is not prepared for recompilation/bailouts. These functions
  //   are never recompiled.
  enum Mode {
    BASE,
    OPTIMIZE,
    NONOPT
  };

  CompilationInfo() : function_(NULL) {}

  void Initialize(Mode mode) {
    mode_ = V8::UseCrankshaft() ? mode : NONOPT;
    if (!shared_info_.is_null() && shared_info_->strict_mode()) {
      MarkAsStrict();
    }
  }

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
  class IsStrict: public BitField<bool, 4, 1> {};

  unsigned flags_;

  // Fields filled in by the compilation pipeline.
  // AST filled in by the parser.
  FunctionLiteral* function_;
  // The scope of the function literal as a convenience.  Set to indicate
  // that scopes have been analyzed.
  Scope* scope_;
  // The compiled code.
  Handle<Code> code_;

  // Possible initial inputs to the compilation process.
  Handle<JSFunction> closure_;
  Handle<SharedFunctionInfo> shared_info_;
  Handle<Script> script_;

  // Fields possibly needed for eager compilation, NULL by default.
  v8::Extension* extension_;
  ScriptDataImpl* pre_parse_data_;

  // The context of the caller is needed for eval code, and will be a null
  // handle otherwise.
  Handle<Context> calling_context_;

  // Compilation mode flag and whether deoptimization is allowed.
  Mode mode_;
  bool supports_deoptimization_;
  int osr_ast_id_;

  DISALLOW_COPY_AND_ASSIGN(CompilationInfo);
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
  // Default maximum number of function optimization attempts before we
  // give up.
  static const int kDefaultMaxOptCount = 10;

  // All routines return a SharedFunctionInfo.
  // If an error occurs an exception is raised and the return handle
  // contains NULL.

  // Compile a String source within a context.
  static Handle<SharedFunctionInfo> Compile(Handle<String> source,
                                            Handle<Object> script_name,
                                            int line_offset,
                                            int column_offset,
                                            v8::Extension* extension,
                                            ScriptDataImpl* pre_data,
                                            Handle<Object> script_data,
                                            NativesFlag is_natives_code);

  // Compile a String source within a context for Eval.
  static Handle<SharedFunctionInfo> CompileEval(Handle<String> source,
                                                Handle<Context> context,
                                                bool is_global,
                                                StrictModeFlag strict_mode);

  // Compile from function info (used for lazy compilation). Returns true on
  // success and false if the compilation resulted in a stack overflow.
  static bool CompileLazy(CompilationInfo* info);

  // Compile a shared function info object (the function is possibly lazily
  // compiled).
  static Handle<SharedFunctionInfo> BuildFunctionInfo(FunctionLiteral* node,
                                                      Handle<Script> script);

  // Set the function info for a newly compiled function.
  static void SetFunctionInfo(Handle<SharedFunctionInfo> function_info,
                              FunctionLiteral* lit,
                              bool is_toplevel,
                              Handle<Script> script);

#ifdef ENABLE_DEBUGGER_SUPPORT
  static bool MakeCodeForLiveEdit(CompilationInfo* info);
#endif

  static void RecordFunctionCompilation(Logger::LogEventsAndTags tag,
                                        CompilationInfo* info,
                                        Handle<SharedFunctionInfo> shared);
};


// During compilation we need a global list of handles to constants
// for frame elements.  When the zone gets deleted, we make sure to
// clear this list of handles as well.
class CompilationZoneScope : public ZoneScope {
 public:
  explicit CompilationZoneScope(ZoneScopeMode mode) : ZoneScope(mode) { }
  virtual ~CompilationZoneScope() {
    if (ShouldDeleteOnExit()) {
      FrameElement::ClearConstantList();
      Result::ClearConstantList();
    }
  }
};


} }  // namespace v8::internal

#endif  // V8_COMPILER_H_
