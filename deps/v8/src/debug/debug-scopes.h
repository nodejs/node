// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEBUG_DEBUG_SCOPES_H_
#define V8_DEBUG_DEBUG_SCOPES_H_

#include "src/debug/debug-frames.h"
#include "src/frames.h"

namespace v8 {
namespace internal {

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
    ScopeTypeModule
  };

  static const int kScopeDetailsTypeIndex = 0;
  static const int kScopeDetailsObjectIndex = 1;
  static const int kScopeDetailsNameIndex = 2;
  static const int kScopeDetailsSize = 3;

  enum Option { DEFAULT, IGNORE_NESTED_SCOPES, COLLECT_NON_LOCALS };

  ScopeIterator(Isolate* isolate, FrameInspector* frame_inspector,
                Option options = DEFAULT);

  ScopeIterator(Isolate* isolate, Handle<JSFunction> function);

  ~ScopeIterator() { delete non_locals_; }

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

  // Populate the list with collected non-local variable names.
  void GetNonLocals(List<Handle<String> >* list_out);

  bool ThisIsNonLocal();

#ifdef DEBUG
  // Debug print of the content of the current scope.
  void DebugPrint();
#endif

 private:
  Isolate* isolate_;
  FrameInspector* const frame_inspector_;
  Handle<Context> context_;
  List<Handle<ScopeInfo> > nested_scope_chain_;
  HashMap* non_locals_;
  bool seen_script_scope_;
  bool failed_;

  inline JavaScriptFrame* GetFrame() {
    return frame_inspector_->GetArgumentsFrame();
  }

  inline Handle<JSFunction> GetFunction() {
    return Handle<JSFunction>(
        JSFunction::cast(frame_inspector_->GetFunction()));
  }

  static bool InternalizedStringMatch(void* key1, void* key2) {
    Handle<String> s1(reinterpret_cast<String**>(key1));
    Handle<String> s2(reinterpret_cast<String**>(key2));
    DCHECK(s1->IsInternalizedString());
    DCHECK(s2->IsInternalizedString());
    return s1.is_identical_to(s2);
  }

  void RetrieveScopeChain(Scope* scope);

  void CollectNonLocals(Scope* scope);

  MUST_USE_RESULT MaybeHandle<JSObject> MaterializeScriptScope();
  MUST_USE_RESULT MaybeHandle<JSObject> MaterializeLocalScope();
  MUST_USE_RESULT MaybeHandle<JSObject> MaterializeModuleScope();
  Handle<JSObject> MaterializeClosure();
  Handle<JSObject> MaterializeCatchScope();
  Handle<JSObject> MaterializeBlockScope();

  bool SetLocalVariableValue(Handle<String> variable_name,
                             Handle<Object> new_value);
  bool SetBlockVariableValue(Handle<String> variable_name,
                             Handle<Object> new_value);
  bool SetClosureVariableValue(Handle<String> variable_name,
                               Handle<Object> new_value);
  bool SetScriptVariableValue(Handle<String> variable_name,
                              Handle<Object> new_value);
  bool SetCatchVariableValue(Handle<String> variable_name,
                             Handle<Object> new_value);
  bool SetContextLocalValue(Handle<ScopeInfo> scope_info,
                            Handle<Context> context,
                            Handle<String> variable_name,
                            Handle<Object> new_value);

  void CopyContextLocalsToScopeObject(Handle<ScopeInfo> scope_info,
                                      Handle<Context> context,
                                      Handle<JSObject> scope_object);
  bool CopyContextExtensionToScopeObject(Handle<JSObject> extension,
                                         Handle<JSObject> scope_object,
                                         JSReceiver::KeyCollectionType type);

  DISALLOW_IMPLICIT_CONSTRUCTORS(ScopeIterator);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_DEBUG_DEBUG_SCOPES_H_
