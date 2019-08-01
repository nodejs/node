// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEBUG_DEBUG_SCOPE_ITERATOR_H_
#define V8_DEBUG_DEBUG_SCOPE_ITERATOR_H_

#include "src/debug/debug-frames.h"
#include "src/debug/debug-interface.h"
#include "src/debug/debug-scopes.h"
#include "src/execution/frames.h"

namespace v8 {
namespace internal {

class DebugScopeIterator final : public debug::ScopeIterator {
 public:
  DebugScopeIterator(Isolate* isolate, FrameInspector* frame_inspector);
  DebugScopeIterator(Isolate* isolate, Handle<JSFunction> function);
  DebugScopeIterator(Isolate* isolate, Handle<JSGeneratorObject> generator);

  bool Done() override;
  void Advance() override;
  ScopeType GetType() override;
  v8::Local<v8::Object> GetObject() override;
  v8::Local<v8::Value> GetFunctionDebugName() override;
  int GetScriptId() override;
  bool HasLocationInfo() override;
  debug::Location GetStartLocation() override;
  debug::Location GetEndLocation() override;

  bool SetVariableValue(v8::Local<v8::String> name,
                        v8::Local<v8::Value> value) override;

 private:
  bool ShouldIgnore();

  v8::internal::ScopeIterator iterator_;
};

class DebugWasmScopeIterator final : public debug::ScopeIterator {
 public:
  DebugWasmScopeIterator(Isolate* isolate, StandardFrame* frame,
                         int inlined_frame_index);

  bool Done() override;
  void Advance() override;
  ScopeType GetType() override;
  v8::Local<v8::Object> GetObject() override;
  v8::Local<v8::Value> GetFunctionDebugName() override;
  int GetScriptId() override;
  bool HasLocationInfo() override;
  debug::Location GetStartLocation() override;
  debug::Location GetEndLocation() override;

  bool SetVariableValue(v8::Local<v8::String> name,
                        v8::Local<v8::Value> value) override;
 private:
  Isolate* isolate_;
  StandardFrame* frame_;
  int inlined_frame_index_;
  ScopeType type_;
};
}  // namespace internal
}  // namespace v8

#endif  // V8_DEBUG_DEBUG_SCOPE_ITERATOR_H_
