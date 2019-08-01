// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEBUG_DEBUG_SCOPES_H_
#define V8_DEBUG_DEBUG_SCOPES_H_

#include <vector>

#include "src/debug/debug-frames.h"
#include "src/execution/frames.h"

namespace v8 {
namespace internal {

class ParseInfo;

// Iterate over the actual scopes visible from a stack frame or from a closure.
// The iteration proceeds from the innermost visible nested scope outwards.
// All scopes are backed by an actual context except the local scope,
// which is inserted "artificially" in the context chain.
class ScopeIterator {
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

  enum Option { DEFAULT, IGNORE_NESTED_SCOPES, COLLECT_NON_LOCALS };

  ScopeIterator(Isolate* isolate, FrameInspector* frame_inspector,
                Option options = DEFAULT);

  ScopeIterator(Isolate* isolate, Handle<JSFunction> function);
  ScopeIterator(Isolate* isolate, Handle<JSGeneratorObject> generator);
  ~ScopeIterator();

  Handle<JSObject> MaterializeScopeDetails();

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

  // Set variable value and return true on success.
  bool SetVariableValue(Handle<String> variable_name, Handle<Object> new_value);

  // Populate the set with collected non-local variable names.
  Handle<StringSet> GetNonLocals();

  // Similar to JSFunction::GetName return the function's name or it's inferred
  // name.
  Handle<Object> GetFunctionDebugName() const;

  Handle<Script> GetScript() const { return script_; }

  bool HasPositionInfo();
  int start_position();
  int end_position();

#ifdef DEBUG
  // Debug print of the content of the current scope.
  void DebugPrint();
#endif

  bool InInnerScope() const { return !function_.is_null(); }
  bool HasContext() const;
  Handle<Context> CurrentContext() const {
    DCHECK(HasContext());
    return context_;
  }

 private:
  Isolate* isolate_;
  ParseInfo* info_ = nullptr;
  FrameInspector* const frame_inspector_ = nullptr;
  Handle<JSGeneratorObject> generator_;
  Handle<JSFunction> function_;
  Handle<Context> context_;
  Handle<Script> script_;
  Handle<StringSet> non_locals_;
  DeclarationScope* closure_scope_ = nullptr;
  Scope* start_scope_ = nullptr;
  Scope* current_scope_ = nullptr;
  bool seen_script_scope_ = false;

  inline JavaScriptFrame* GetFrame() const {
    return frame_inspector_->javascript_frame();
  }

  int GetSourcePosition();

  void TryParseAndRetrieveScopes(ScopeIterator::Option option);

  void RetrieveScopeChain(DeclarationScope* scope);

  void UnwrapEvaluationContext();

  using Visitor =
      std::function<bool(Handle<String> name, Handle<Object> value)>;

  Handle<JSObject> WithContextExtension();

  bool SetLocalVariableValue(Handle<String> variable_name,
                             Handle<Object> new_value);
  bool SetContextVariableValue(Handle<String> variable_name,
                               Handle<Object> new_value);
  bool SetContextExtensionValue(Handle<String> variable_name,
                                Handle<Object> new_value);
  bool SetScriptVariableValue(Handle<String> variable_name,
                              Handle<Object> new_value);
  bool SetModuleVariableValue(Handle<String> variable_name,
                              Handle<Object> new_value);

  // Helper functions.
  void VisitScope(const Visitor& visitor, Mode mode) const;
  void VisitLocalScope(const Visitor& visitor, Mode mode) const;
  void VisitScriptScope(const Visitor& visitor) const;
  void VisitModuleScope(const Visitor& visitor) const;
  bool VisitLocals(const Visitor& visitor, Mode mode) const;
  bool VisitContextLocals(const Visitor& visitor, Handle<ScopeInfo> scope_info,
                          Handle<Context> context) const;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ScopeIterator);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_DEBUG_DEBUG_SCOPES_H_
