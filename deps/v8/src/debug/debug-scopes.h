// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEBUG_DEBUG_SCOPES_H_
#define V8_DEBUG_DEBUG_SCOPES_H_

#include "src/debug/debug-frames.h"
#include "src/debug/debug-scope-info.h"

namespace v8 {
namespace internal {

class JavaScriptFrame;

// Iterate over the actual scopes visible from a stack frame or from a closure.
// The iteration proceeds from the innermost visible nested scope outwards.
// All scopes are backed by an actual context except the local scope,
// which is inserted "artificially" in the context chain.
class V8_EXPORT_PRIVATE ScopeIterator {
 public:
  enum ScopeType {
    ScopeTypeGlobal = 0,
    ScopeTypeLocal,
    ScopeTypeWith,
    ScopeTypeClosure,
    ScopeTypeCatch,
    ScopeTypeBlock,
    ScopeTypeScript,
    ScopeTypeEval,
    ScopeTypeModule
  };

  static const int kScopeDetailsTypeIndex = 0;
  static const int kScopeDetailsObjectIndex = 1;
  static const int kScopeDetailsNameIndex = 2;
  static const int kScopeDetailsStartPositionIndex = 3;
  static const int kScopeDetailsEndPositionIndex = 4;
  static const int kScopeDetailsFunctionIndex = 5;
  static const int kScopeDetailsSize = 6;

  ScopeIterator(Isolate* isolate, FrameInspector* frame_inspector);

  ScopeIterator(Isolate* isolate, DirectHandle<JSFunction> function);
  ScopeIterator(Isolate* isolate, Handle<JSGeneratorObject> generator);
  ~ScopeIterator();

  DirectHandle<JSObject> MaterializeScopeDetails();

  // More scopes?
  bool Done() const { return context_.is_null(); }

  // Move to the next scope.
  void Next();

  // Restart to the first scope and context.
  void Restart();

  // Return the type of the current scope.
  ScopeType Type() const;

  // Indicates which variables should be visited. Either only variables from the
  // scope that are available on the stack, or all variables.
  enum class Mode { STACK, ALL };

  // Return the JavaScript object with the content of the current scope.
  Handle<JSObject> ScopeObject(Mode mode);

  // Returns whether the current scope declares any variables.
  bool DeclaresLocals(Mode mode) const;

  enum class VariableInfo {
    // The scope does not declare any variables.
    kEmpty,
    // The scope declares variables, but none of their values are available
    // (i.e. all of them are optimized out or in their TDZ).
    kAllUnavailable,
    // The scope declares at least one variable with an available value.
    kAvailable,
  };

  // Classifies the variables declared by the current scope.
  VariableInfo GetVariableInfo(Mode mode) const;

  // Returns whether the current scope should be ignored by debugger scope
  // numbers.
  bool ShouldIgnore() const;

  // Advances the iterator by the given scope number, skipping ignored scopes.
  // Returns true if the scope was found, false otherwise.
  bool AdvanceToScopeNumber(int scope_number = 0);

  // Set variable value and return true on success.
  bool SetVariableValue(Handle<String> variable_name,
                        DirectHandle<Object> new_value);

  bool ClosureScopeHasThisReference() const;

  // Similar to JSFunction::GetName return the function's name or it's inferred
  // name.
  DirectHandle<Object> GetFunctionDebugName() const;

  DirectHandle<Script> GetScript() const { return script_; }

  bool HasPositionInfo();
  int start_position();
  int end_position();

#ifdef DEBUG
  // Debug print of the content of the current scope.
  void DebugPrint();
#endif

  // Whether the current scope is backed by a scope in `debug_scope_info_`.
  bool HasScope() const { return current_scope_index_ != -1; }
  // Whether the current scope is backed by `debug_scope_info_` and belongs to
  // the paused function (i.e. its stack-allocated variables are available).
  bool InInnerScope() const {
    return !function_.is_null() && HasScope();
  }
  bool HasContext() const;
  bool InPausedFunctionScopes() const;
  // Whether `context_` belongs to the current script (`script_`).
  //
  // ScopeIterator walks the static DebugScriptScope tree (`current_scope()` in
  // `script_`) in lockstep with the runtime Context chain (`context_`), and
  // HasContext() matches a context-requiring static scope to `context_` by
  // comparing UniqueIdInScript().
  //
  // UniqueIdInScript() is only unique within a single script (character start
  // offset for ordinary scopes, or -2 for both EVAL_SCOPE and MODULE_SCOPE).
  // When execution pauses before an inner scope's runtime context has been
  // pushed (or after all contexts created inside an eval / new Function script
  // have been unwound while `current_scope()` has not yet crossed
  // FindEvalOuterScope()), `context_` already points to an enclosing runtime
  // context created outside the paused function or by an outer/caller script.
  //
  // Returns true iff `context_` was allocated for a scope in `script_` (and,
  // while still inside InPausedFunctionScopes(), inside the paused function
  // itself rather than its outer creation context `function_->context()`,
  // except for top-level script/module code which executes directly in its own
  // ScriptContext/ModuleContext), making UniqueIdInScript() comparison with
  // `current_scope()` meaningful.
  bool ContextBelongsToCurrentScript() const;
  bool NeedsContext() const;
  bool NeedsAndHasContext() const { return NeedsContext() && HasContext(); }
  Handle<Context> CurrentContext() const {
    DCHECK(HasContext());
    return context_;
  }
  std::optional<DebugScriptScope> CurrentDebugScope() const {
    if (current_scope_index_ == -1) return std::nullopt;
    return current_scope();
  }
  // Returns the innermost runtime context at the current position. Unlike
  // CurrentContext(), this is also valid for scopes without their own context.
  Handle<Context> EvaluationContext() const { return context_; }

 private:
  Isolate* isolate_;
  FrameInspector* const frame_inspector_ = nullptr;
  Handle<JSGeneratorObject> generator_;

  // The currently-executing function from the inspected frame, or null if this
  // ScopeIterator has already iterated to any Scope outside that function.
  Handle<JSFunction> function_;

  Handle<Context> context_;
  // The script and serialized scope tree containing the current scope. These
  // start out as the paused script and move on to the caller's script when
  // iteration leaves the root scope of a direct eval script.
  // `current_scope_index_` indexes into `debug_scope_info_` and is set to -1
  // once iteration leaves the outermost scope tree.
  // `start_scope_index_` and `closure_scope_index_` always index into the
  // paused script's scope tree.
  Handle<Script> script_;
  Handle<DebugScriptScopeInfo> debug_scope_info_;
  int start_scope_index_ = -1;
  int closure_scope_index_ = -1;
  int current_scope_index_ = -1;
  bool seen_script_scope_ = false;
  // Set when paused at a return position. Iteration then starts at the
  // closure scope and nested block, catch and with scopes are skipped.
  bool ignore_nested_scopes_ = false;

  DebugScriptScope current_scope() const {
    return DebugScriptScope::FromIndex(debug_scope_info_, current_scope_index_);
  }
  DebugScriptScope closure_scope() const {
    return DebugScriptScope::FromIndex(debug_scope_info_, closure_scope_index_);
  }

  inline JavaScriptFrame* GetFrame() const {
    return frame_inspector_->javascript_frame();
  }

  void AdvanceOneContext();
  void AdvanceScope();
  void AdvanceContext();

  int GetSourcePosition() const;

  void TryParseAndRetrieveScopes();
  void UnwindToClosureContext();

  static Tagged<Context> UnwrapEvaluationContext(Tagged<Context> context);
  void UnwrapEvaluationContext();

  using Visitor =
      std::function<bool(DirectHandle<String> name, DirectHandle<Object> value,
                         ScopeType scope_type)>;

  Handle<JSObject> WithContextExtension();

  // Sets the value of a local variable in the current scope.
  // Note on TDZ handling:
  // - Context, script, and module lexical slots never elide TdzHole
  //   initialization, and writing over TdzHole via Context::Set would wrap a
  //   kNotAssigned (NoCell) slot in a ContextCell (leaking a raw ContextCell to
  //   JS).
  // - Stack and suspended-generator VariableLocation::LOCAL registers only emit
  //   TdzHole when a path before or across the initializer can read the
  //   register (Variable::binding_needs_init()); otherwise TdzHole
  //   initialization is elided and the register holds `undefined` until the
  //   initializer runs.
  // - Calling IsLocalInTdz here falsely rejected valid debugger writes to
  //   already-initialized `undefined` locals (such as in switch scopes where
  //   initializer_position is kMaxInt, or earlier bindings in multi-binding
  //   destructuring declarations where initializer_position is the end of the
  //   declaration).
  // - Working as intended (WAI): While overwriting `undefined` with another
  //   value in an elided-TDZ register via SetLocalVariableValue is
  //   theoretically observable (e.g. to a subsequent debugger read via
  //   VisitLocals / DebugEvaluate::Local before the initializer runs, since
  //   IsLocalInTdz only checks IsUndefined(value)), it cannot violate VM,
  //   compiler, deoptimization, or ContextCell invariants and is overwritten
  //   when the initializer executes. We intentionally do not care about this
  //   edge case and consider it working as intended.
  bool SetLocalVariableValue(DirectHandle<String> variable_name,
                             DirectHandle<Object> new_value);
  bool SetContextVariableValue(DirectHandle<String> variable_name,
                               DirectHandle<Object> new_value);
  bool SetContextExtensionValue(DirectHandle<String> variable_name,
                                DirectHandle<Object> new_value);
  bool SetScriptVariableValue(DirectHandle<String> variable_name,
                              DirectHandle<Object> new_value);
  bool SetModuleVariableValue(DirectHandle<String> variable_name,
                              DirectHandle<Object> new_value);

  // Helper functions.
  void VisitScope(const Visitor& visitor, Mode mode) const;
  void VisitLocalScope(const Visitor& visitor, Mode mode,
                       ScopeType scope_type) const;
  void VisitScriptScope(const Visitor& visitor) const;
  void VisitModuleScope(const Visitor& visitor) const;
  // Checks whether a local/register slot is currently in its temporal dead
  // zone. When hole-initialization bytecodes are elided
  // (var.is_hole_initialization_elided), this uses the static source position
  // heuristic (GetSourcePosition() <= var.initializer_position). Note:
  // 1. Switch statements: ParserBase::ParseSwitchStatement forces hole
  //    initialization for all lexical locals in a switch scope so
  //    var.is_hole_initialization_elided is false and bytecode always emits
  //    explicit LdaTdzHole at scope entry. Uninitialized switch locals
  //    therefore always hold an actual TdzHole, and an `undefined` runtime
  //    value is always an initialized value.
  // 2. Multi-binding destructuring: When hole initialization is elided,
  //    IsLocalInTdz intentionally uses the lightweight static
  //    GetSourcePosition() <= var.initializer_position heuristic rather than
  //    scanning bytecode. For multi-binding destructuring patterns (e.g.
  //    `let [a, b = (debugger, 2)] = [undefined]`), the parser assigns the end
  //    position of the entire declaration to all bindings in the pattern, so
  //    earlier bindings initialized to `undefined` conservatively appear as
  //    unavailable until the declaration finishes. This is working as
  //    intended.
  bool IsLocalInTdz(const DebugVariableInfo& var, Tagged<Object> value) const;
  bool VisitLocals(const Visitor& visitor, Mode mode,
                   ScopeType scope_type) const;
  bool VisitContextLocals(const Visitor& visitor,
                          DirectHandle<ScopeInfo> scope_info,
                          DirectHandle<Context> context,
                          ScopeType scope_type) const;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ScopeIterator);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_DEBUG_DEBUG_SCOPES_H_
