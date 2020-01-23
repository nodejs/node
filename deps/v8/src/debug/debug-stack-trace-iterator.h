// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEBUG_DEBUG_STACK_TRACE_ITERATOR_H_
#define V8_DEBUG_DEBUG_STACK_TRACE_ITERATOR_H_

#include <memory>

#include "src/debug/debug-frames.h"
#include "src/debug/debug-interface.h"
#include "src/execution/frames.h"

namespace v8 {
namespace internal {

class DebugStackTraceIterator final : public debug::StackTraceIterator {
 public:
  DebugStackTraceIterator(Isolate* isolate, int index);
  ~DebugStackTraceIterator() override;

  bool Done() const override;
  void Advance() override;

  int GetContextId() const override;
  v8::MaybeLocal<v8::Value> GetReceiver() const override;
  v8::Local<v8::Value> GetReturnValue() const override;
  v8::Local<v8::String> GetFunctionDebugName() const override;
  v8::Local<v8::debug::Script> GetScript() const override;
  debug::Location GetSourceLocation() const override;
  v8::Local<v8::Function> GetFunction() const override;
  std::unique_ptr<v8::debug::ScopeIterator> GetScopeIterator() const override;

  bool Restart() override;
  v8::MaybeLocal<v8::Value> Evaluate(v8::Local<v8::String> source,
                                     bool throw_on_side_effect) override;

 private:
  Isolate* isolate_;
  StackTraceFrameIterator iterator_;
  std::unique_ptr<FrameInspector> frame_inspector_;
  int inlined_frame_index_;
  bool is_top_frame_;
};
}  // namespace internal
}  // namespace v8

#endif  // V8_DEBUG_DEBUG_STACK_TRACE_ITERATOR_H_
