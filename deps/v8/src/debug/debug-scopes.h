// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEBUG_DEBUG_SCOPES_H_
#define V8_DEBUG_DEBUG_SCOPES_H_

#include "src/debug/debug-frames.h"
#include "src/frames.h"

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

  MUST_USE_RESULT MaybeHandle<JSObject> MaterializeScopeDetails();

  // More scopes?
  bool Done() {
    DCHECK(!failed_);
    return context_.is_null();
  }

  bool Failed() { return failed_; }

  // Move to the next scope.
  void Next();

  // Return the type of the current scope.
  ScopeType Type();

  // Return the JavaScript object with the content of the current scope.
  MaybeHandle<JSObject> ScopeObject();

  bool HasContext();

  // Set variable value and return true on success.
  bool SetVariableValue(Handle<String> variable_name, Handle<Object> new_value);

  Handle<ScopeInfo> CurrentScopeInfo();

  // Return the context for this scope. For the local context there might not
  // be an actual context.
  Handle<Context> CurrentContext();

  // Populate the set with collected non-local variable names.
  Handle<StringSet> GetNonLocals();

#ifdef DEBUG
  // Debug print of the content of the current scope.
  void DebugPrint();
#endif

 private:
  struct ExtendedScopeInfo {
    ExtendedScopeInfo(Handle<ScopeInfo> info, int start, int end)
        : scope_info(info), start_position(start), end_position(end) {}
    explicit ExtendedScopeInfo(Handle<ScopeInfo> info)
        : scope_info(info), start_position(-1), end_position(-1) {}
    Handle<ScopeInfo> scope_info;
    int start_position;
    int end_position;
    bool is_hidden() { return start_position == -1 && end_position == -1; }
  };

  Isolate* isolate_;
  FrameInspector* const frame_inspector_;
  Handle<Context> context_;
  List<ExtendedScopeInfo> nested_scope_chain_;
  Handle<StringSet> non_locals_;
  bool seen_script_scope_;
  bool failed_;

  inline JavaScriptFrame* GetFrame() {
    return frame_inspector_->GetArgumentsFrame();
  }

  inline Handle<JSFunction> GetFunction() {
    return frame_inspector_->GetFunction();
  }

  void RetrieveScopeChain(DeclarationScope* scope);

  void CollectNonLocals(ParseInfo* info, DeclarationScope* scope);

  void UnwrapEvaluationContext();

  MUST_USE_RESULT MaybeHandle<JSObject> MaterializeScriptScope();
  MUST_USE_RESULT MaybeHandle<JSObject> MaterializeLocalScope();
  MUST_USE_RESULT MaybeHandle<JSObject> MaterializeModuleScope();
  Handle<JSObject> MaterializeClosure();
  Handle<JSObject> MaterializeCatchScope();
  Handle<JSObject> MaterializeInnerScope();
  Handle<JSObject> WithContextExtension();

  bool SetLocalVariableValue(Handle<String> variable_name,
                             Handle<Object> new_value);
  bool SetInnerScopeVariableValue(Handle<String> variable_name,
                                  Handle<Object> new_value);
  bool SetClosureVariableValue(Handle<String> variable_name,
                               Handle<Object> new_value);
  bool SetScriptVariableValue(Handle<String> variable_name,
                              Handle<Object> new_value);
  bool SetCatchVariableValue(Handle<String> variable_name,
                             Handle<Object> new_value);

  // Helper functions.
  bool SetParameterValue(Handle<ScopeInfo> scope_info, JavaScriptFrame* frame,
                         Handle<String> parameter_name,
                         Handle<Object> new_value);
  bool SetStackVariableValue(Handle<ScopeInfo> scope_info,
                             Handle<String> variable_name,
                             Handle<Object> new_value);
  bool SetContextVariableValue(Handle<ScopeInfo> scope_info,
                               Handle<Context> context,
                               Handle<String> variable_name,
                               Handle<Object> new_value);

  void CopyContextLocalsToScopeObject(Handle<ScopeInfo> scope_info,
                                      Handle<Context> context,
                                      Handle<JSObject> scope_object);
  void CopyContextExtensionToScopeObject(Handle<Context> context,
                                         Handle<JSObject> scope_object,
                                         KeyCollectionMode mode);

  // Get the chain of nested scopes within this scope for the source statement
  // position. The scopes will be added to the list from the outermost scope to
  // the innermost scope. Only nested block, catch or with scopes are tracked
  // and will be returned, but no inner function scopes.
  void GetNestedScopeChain(Isolate* isolate, Scope* scope,
                           int statement_position);

  DISALLOW_IMPLICIT_CONSTRUCTORS(ScopeIterator);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_DEBUG_DEBUG_SCOPES_H_
