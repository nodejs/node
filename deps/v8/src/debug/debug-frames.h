// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEBUG_DEBUG_FRAMES_H_
#define V8_DEBUG_DEBUG_FRAMES_H_

#include <memory>

#include "src/deoptimizer/deoptimizer.h"
#include "src/execution/isolate.h"
#include "src/execution/v8threads.h"
#include "src/objects/objects.h"

namespace v8 {
namespace internal {

class JavaScriptFrame;
class StandardFrame;
class WasmFrame;

class FrameInspector {
 public:
  FrameInspector(StandardFrame* frame, int inlined_frame_index,
                 Isolate* isolate);

  ~FrameInspector();

  int GetParametersCount();
  Handle<JSFunction> GetFunction() const { return function_; }
  Handle<Script> GetScript() { return script_; }
  Handle<Object> GetParameter(int index);
  Handle<Object> GetExpression(int index);
  int GetSourcePosition() { return source_position_; }
  bool IsConstructor() { return is_constructor_; }
  Handle<Object> GetContext();
  Handle<Object> GetReceiver() { return receiver_; }

  Handle<String> GetFunctionName() { return function_name_; }

  bool IsWasm();
  bool IsJavaScript();

  JavaScriptFrame* javascript_frame();

  int inlined_frame_index() const { return inlined_frame_index_; }

 private:
  bool ParameterIsShadowedByContextLocal(Handle<ScopeInfo> info,
                                         Handle<String> parameter_name);

  StandardFrame* frame_;
  int inlined_frame_index_;
  std::unique_ptr<DeoptimizedFrameInfo> deoptimized_frame_;
  Isolate* isolate_;
  Handle<Script> script_;
  Handle<Object> receiver_;
  Handle<JSFunction> function_;
  Handle<String> function_name_;
  int source_position_ = -1;
  bool is_optimized_ = false;
  bool is_interpreted_ = false;
  bool has_adapted_arguments_ = false;
  bool is_constructor_ = false;

  DISALLOW_COPY_AND_ASSIGN(FrameInspector);
};

class RedirectActiveFunctions : public ThreadVisitor {
 public:
  enum class Mode {
    kUseOriginalBytecode,
    kUseDebugBytecode,
  };

  explicit RedirectActiveFunctions(SharedFunctionInfo shared, Mode mode);

  void VisitThread(Isolate* isolate, ThreadLocalTop* top) override;

 private:
  SharedFunctionInfo shared_;
  Mode mode_;
  DisallowHeapAllocation no_gc_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_DEBUG_DEBUG_FRAMES_H_
