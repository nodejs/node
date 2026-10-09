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

  DebugScriptScope current_scope() const {
    return DebugScriptScope::FromIndex(debug_scope_info_, current_scope_index_);
  }
  DebugScriptScope closure_scope() const {
    return DebugScriptScope::FromIndex(debug_scope_info_, closure_scope_index_);
  }

  inline JavaScriptFrame* GetFrame() const {
    return frame_inspector_->javascript_frame();
  }

  bool AdvanceOneScope();
  void AdvanceOneContext();
  void AdvanceScope();
  void AdvanceContext();

  int GetSourcePosition() const;

  void TryParseAndRetrieveScopes();

  void UnwrapEvaluationContext();

  using Visitor =
      std::function<bool(DirectHandle<String> name, DirectHandle<Object> value,
                         ScopeType scope_type)>;

  Handle<JSObject> WithContextExtension();

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
