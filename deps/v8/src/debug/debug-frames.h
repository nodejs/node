// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEBUG_DEBUG_FRAMES_H_
#define V8_DEBUG_DEBUG_FRAMES_H_

#include "src/deoptimizer.h"
#include "src/frames.h"
#include "src/isolate.h"
#include "src/objects.h"
#include "src/wasm/wasm-interpreter.h"

namespace v8 {
namespace internal {

class FrameInspector {
 public:
  FrameInspector(StandardFrame* frame, int inlined_frame_index,
                 Isolate* isolate);

  ~FrameInspector();

  int GetParametersCount();
  Handle<JSFunction> GetFunction() { return function_; }
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

  inline JavaScriptFrame* javascript_frame() {
    return frame_->is_arguments_adaptor() ? ArgumentsAdaptorFrame::cast(frame_)
                                          : JavaScriptFrame::cast(frame_);
  }

  JavaScriptFrame* GetArgumentsFrame() { return javascript_frame(); }
  void SetArgumentsFrame(StandardFrame* frame);

  void MaterializeStackLocals(Handle<JSObject> target,
                              Handle<ScopeInfo> scope_info,
                              bool materialize_arguments_object = false);

  void UpdateStackLocalsFromMaterializedObject(Handle<JSObject> object,
                                               Handle<ScopeInfo> scope_info);

 private:
  bool ParameterIsShadowedByContextLocal(Handle<ScopeInfo> info,
                                         Handle<String> parameter_name);

  StandardFrame* frame_;
  std::unique_ptr<DeoptimizedFrameInfo> deoptimized_frame_;
  wasm::WasmInterpreter::FramePtr wasm_interpreted_frame_;
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


class DebugFrameHelper : public AllStatic {
 public:
  static SaveContext* FindSavedContextForFrame(Isolate* isolate,
                                               StandardFrame* frame);
  // Advances the iterator to the frame that matches the index and returns the
  // inlined frame index, or -1 if not found.  Skips native JS functions.
  static int FindIndexedNonNativeFrame(StackTraceFrameIterator* it, int index);

  // Helper functions for wrapping and unwrapping stack frame ids.
  static Smi* WrapFrameId(StackFrame::Id id) {
    DCHECK(IsAligned(OffsetFrom(id), static_cast<intptr_t>(4)));
    return Smi::FromInt(id >> 2);
  }

  static StackFrame::Id UnwrapFrameId(int wrapped) {
    return static_cast<StackFrame::Id>(wrapped << 2);
  }
};

}  // namespace internal
}  // namespace v8

#endif  // V8_DEBUG_DEBUG_FRAMES_H_
