// Copyright 2006-2008 the V8 project authors. All rights reserved.
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
#include "parser.h"
#include "register-allocator.h"
#include "zone.h"

namespace v8 {
namespace internal {

// CompilationInfo encapsulates some information known at compile time.  It
// is constructed based on the resources available at compile-time.
class CompilationInfo BASE_EMBEDDED {
 public:
  virtual ~CompilationInfo() {}

  // Dispatched behavior.
  virtual Handle<SharedFunctionInfo> shared_info() const = 0;

  virtual Handle<Script> script() const {
    return Handle<Script>(Script::cast(shared_info()->script()));
  }

  virtual Handle<JSFunction> closure() const {
    return Handle<JSFunction>::null();
  }

  virtual bool is_eval() const { return false; }

  virtual int loop_nesting() const { return 0; }

  virtual bool has_global_object() const { return false; }
  virtual GlobalObject* global_object() const { return NULL; }

  // There should always be a function literal, but it may be set after
  // construction (for lazy compilation).
  FunctionLiteral* function() { return function_; }
  void set_function(FunctionLiteral* literal) { function_ = literal; }

  // Derived accessors.
  Scope* scope() { return function()->scope(); }

 protected:
  CompilationInfo() : function_(NULL) {}

 private:
  FunctionLiteral* function_;

  DISALLOW_COPY_AND_ASSIGN(CompilationInfo);
};


class EagerCompilationInfo: public CompilationInfo {
 public:
  EagerCompilationInfo(Handle<Script> script, bool is_eval)
      : script_(script), is_eval_(is_eval) {
    ASSERT(!script.is_null());
  }

  // Overridden functions from the base class.
  virtual Handle<SharedFunctionInfo> shared_info() const {
    return Handle<SharedFunctionInfo>::null();
  }

  virtual Handle<Script> script() const { return script_; }

  virtual bool is_eval() const { return is_eval_; }

 private:
  Handle<Script> script_;
  bool is_eval_;
};


class LazySharedCompilationInfo: public CompilationInfo {
 public:
  explicit LazySharedCompilationInfo(Handle<SharedFunctionInfo> shared_info)
      : shared_info_(shared_info) {
    ASSERT(!shared_info.is_null());
  }

  // Overridden functions from the base class.
  virtual Handle<SharedFunctionInfo> shared_info() const {
    return shared_info_;
  }

 private:
  Handle<SharedFunctionInfo> shared_info_;
};


class LazyFunctionCompilationInfo: public CompilationInfo {
 public:
  LazyFunctionCompilationInfo(Handle<JSFunction> closure,
                              int loop_nesting)
      : closure_(closure), loop_nesting_(loop_nesting) {
    ASSERT(!closure.is_null());
  }

  // Overridden functions from the base class.
  virtual Handle<SharedFunctionInfo> shared_info() const {
    return Handle<SharedFunctionInfo>(closure_->shared());
  }

  virtual int loop_nesting() const { return loop_nesting_; }

  virtual bool has_global_object() const {
    return closure_->context()->global() != NULL;
  }

  virtual GlobalObject* global_object() const {
    return closure_->context()->global();
  }

 private:
  Handle<JSFunction> closure_;
  int loop_nesting_;
};


// The V8 compiler
//
// General strategy: Source code is translated into an anonymous function w/o
// parameters which then can be executed. If the source code contains other
// functions, they will be compiled and allocated as part of the compilation
// of the source code.

// Please note this interface returns shared function infos.
// This means you need to call Factory::NewFunctionFromSharedFunctionInfo
// before you have a real function with a context.

class Compiler : public AllStatic {
 public:
  enum ValidationState { VALIDATE_JSON, DONT_VALIDATE_JSON };

  // All routines return a JSFunction.
  // If an error occurs an exception is raised and
  // the return handle contains NULL.

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
                                                ValidationState validation);

  // Compile from function info (used for lazy compilation). Returns
  // true on success and false if the compilation resulted in a stack
  // overflow.
  static bool CompileLazy(CompilationInfo* info);

  // Compile a shared function info object (the function is possibly
  // lazily compiled). Called recursively from a backend code
  // generator 'caller' to build the shared function info.
  static Handle<SharedFunctionInfo> BuildFunctionInfo(FunctionLiteral* node,
                                                      Handle<Script> script,
                                                      AstVisitor* caller);

  // Set the function info for a newly compiled function.
  static void SetFunctionInfo(Handle<SharedFunctionInfo> function_info,
                              FunctionLiteral* lit,
                              bool is_toplevel,
                              Handle<Script> script);

 private:
  static void RecordFunctionCompilation(Logger::LogEventsAndTags tag,
                                        Handle<String> name,
                                        Handle<String> inferred_name,
                                        int start_position,
                                        Handle<Script> script,
                                        Handle<Code> code);
};


#ifdef ENABLE_DEBUGGER_SUPPORT

Handle<Code> MakeCodeForLiveEdit(CompilationInfo* info);

#endif


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
