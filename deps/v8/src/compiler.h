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
  // Compilation mode.  Either the compiler is used as the primary
  // compiler and needs to setup everything or the compiler is used as
  // the secondary compiler for split compilation and has to handle
  // bailouts.
  enum Mode {
    PRIMARY,
    SECONDARY
  };

  // A description of the compilation state at a bailout to the secondary
  // code generator.
  //
  // The state is currently simple: there are no parameters or local
  // variables to worry about ('this' can be found in the stack frame).
  // There are at most two live values.
  //
  // There is a label that should be bound to the beginning of the bailout
  // stub code.
  class Bailout : public ZoneObject {
   public:
    Bailout(Register left, Register right) : left_(left), right_(right) {}

    Label* label() { return &label_; }

   private:
    Register left_;
    Register right_;
    Label label_;
  };


  // Lazy compilation of a JSFunction.
  CompilationInfo(Handle<JSFunction> closure,
                  int loop_nesting,
                  Handle<Object> receiver)
      : closure_(closure),
        function_(NULL),
        is_eval_(false),
        loop_nesting_(loop_nesting),
        receiver_(receiver) {
    Initialize();
    ASSERT(!closure_.is_null() &&
           shared_info_.is_null() &&
           script_.is_null());
  }

  // Lazy compilation based on SharedFunctionInfo.
  explicit CompilationInfo(Handle<SharedFunctionInfo> shared_info)
      : shared_info_(shared_info),
        function_(NULL),
        is_eval_(false),
        loop_nesting_(0) {
    Initialize();
    ASSERT(closure_.is_null() &&
           !shared_info_.is_null() &&
           script_.is_null());
  }

  // Eager compilation.
  CompilationInfo(FunctionLiteral* literal, Handle<Script> script, bool is_eval)
      : script_(script),
        function_(literal),
        is_eval_(is_eval),
        loop_nesting_(0) {
    Initialize();
    ASSERT(closure_.is_null() &&
           shared_info_.is_null() &&
           !script_.is_null());
  }

  // We can only get a JSFunction if we actually have one.
  Handle<JSFunction> closure() { return closure_; }

  // We can get a SharedFunctionInfo from a JSFunction or if we actually
  // have one.
  Handle<SharedFunctionInfo> shared_info() {
    if (!closure().is_null()) {
      return Handle<SharedFunctionInfo>(closure()->shared());
    } else {
      return shared_info_;
    }
  }

  // We can always get a script.  Either we have one or we can get a shared
  // function info.
  Handle<Script> script() {
    if (!script_.is_null()) {
      return script_;
    } else {
      ASSERT(shared_info()->script()->IsScript());
      return Handle<Script>(Script::cast(shared_info()->script()));
    }
  }

  // There should always be a function literal, but it may be set after
  // construction (for lazy compilation).
  FunctionLiteral* function() { return function_; }
  void set_function(FunctionLiteral* literal) {
    ASSERT(function_ == NULL);
    function_ = literal;
  }

  // Simple accessors.
  bool is_eval() { return is_eval_; }
  int loop_nesting() { return loop_nesting_; }
  bool has_receiver() { return !receiver_.is_null(); }
  Handle<Object> receiver() { return receiver_; }
  List<Bailout*>* bailouts() { return &bailouts_; }

  // Accessors for mutable fields (possibly set by analysis passes) with
  // default values given by Initialize.
  Mode mode() { return mode_; }
  void set_mode(Mode mode) { mode_ = mode; }

  bool has_this_properties() { return has_this_properties_; }
  void set_has_this_properties(bool flag) { has_this_properties_ = flag; }

  bool has_global_object() {
    return !closure().is_null() && (closure()->context()->global() != NULL);
  }

  GlobalObject* global_object() {
    return has_global_object() ? closure()->context()->global() : NULL;
  }

  bool has_globals() { return has_globals_; }
  void set_has_globals(bool flag) { has_globals_ = flag; }

  // Derived accessors.
  Scope* scope() { return function()->scope(); }

  // Add a bailout with two live values.
  Label* AddBailout(Register left, Register right) {
    Bailout* bailout = new Bailout(left, right);
    bailouts_.Add(bailout);
    return bailout->label();
  }

  // Add a bailout with no live values.
  Label* AddBailout() { return AddBailout(no_reg, no_reg); }

 private:
  void Initialize() {
    mode_ = PRIMARY;
    has_this_properties_ = false;
    has_globals_ = false;
  }

  Handle<JSFunction> closure_;
  Handle<SharedFunctionInfo> shared_info_;
  Handle<Script> script_;

  FunctionLiteral* function_;
  Mode mode_;

  bool is_eval_;
  int loop_nesting_;

  Handle<Object> receiver_;

  bool has_this_properties_;
  bool has_globals_;

  // An ordered list of bailout points encountered during fast-path
  // compilation.
  List<Bailout*> bailouts_;

  DISALLOW_COPY_AND_ASSIGN(CompilationInfo);
};


// The V8 compiler
//
// General strategy: Source code is translated into an anonymous function w/o
// parameters which then can be executed. If the source code contains other
// functions, they will be compiled and allocated as part of the compilation
// of the source code.

// Please note this interface returns function boilerplates.
// This means you need to call Factory::NewFunctionFromBoilerplate
// before you have a real function with context.

class Compiler : public AllStatic {
 public:
  enum ValidationState { VALIDATE_JSON, DONT_VALIDATE_JSON };

  // All routines return a JSFunction.
  // If an error occurs an exception is raised and
  // the return handle contains NULL.

  // Compile a String source within a context.
  static Handle<JSFunction> Compile(Handle<String> source,
                                    Handle<Object> script_name,
                                    int line_offset, int column_offset,
                                    v8::Extension* extension,
                                    ScriptDataImpl* pre_data,
                                    Handle<Object> script_data);

  // Compile a String source within a context for Eval.
  static Handle<JSFunction> CompileEval(Handle<String> source,
                                        Handle<Context> context,
                                        bool is_global,
                                        ValidationState validation);

  // Compile from function info (used for lazy compilation). Returns
  // true on success and false if the compilation resulted in a stack
  // overflow.
  static bool CompileLazy(CompilationInfo* info);

  // Compile a function boilerplate object (the function is possibly
  // lazily compiled). Called recursively from a backend code
  // generator 'caller' to build the boilerplate.
  static Handle<JSFunction> BuildBoilerplate(FunctionLiteral* node,
                                             Handle<Script> script,
                                             AstVisitor* caller);

  // Set the function info for a newly compiled function.
  static void SetFunctionInfo(Handle<JSFunction> fun,
                              FunctionLiteral* lit,
                              bool is_toplevel,
                              Handle<Script> script);

 private:

#if defined ENABLE_LOGGING_AND_PROFILING || defined ENABLE_OPROFILE_AGENT
  static void LogCodeCreateEvent(Logger::LogEventsAndTags tag,
                                 Handle<String> name,
                                 Handle<String> inferred_name,
                                 int start_position,
                                 Handle<Script> script,
                                 Handle<Code> code);
#endif
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
