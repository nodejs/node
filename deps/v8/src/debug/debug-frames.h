// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEBUG_DEBUG_FRAMES_H_
#define V8_DEBUG_DEBUG_FRAMES_H_

#include "src/deoptimizer.h"
#include "src/frames.h"
#include "src/isolate.h"
#include "src/objects.h"

namespace v8 {
namespace internal {

// Forward declaration:
namespace wasm {
class InterpretedFrame;
}

class FrameInspector {
 public:
  FrameInspector(StandardFrame* frame, int inlined_frame_index,
                 Isolate* isolate);

  ~FrameInspector();

  FrameSummary& summary() { return frame_summary_; }

  int GetParametersCount();
  Handle<JSFunction> GetFunction();
  Handle<Script> GetScript();
  Handle<Object> GetParameter(int index);
  Handle<Object> GetExpression(int index);
  int GetSourcePosition();
  bool IsConstructor();
  Handle<Object> GetContext();

  inline JavaScriptFrame* javascript_frame() {
    return frame_->is_arguments_adaptor() ? ArgumentsAdaptorFrame::cast(frame_)
                                          : JavaScriptFrame::cast(frame_);
  }

  JavaScriptFrame* GetArgumentsFrame() { return javascript_frame(); }
  void SetArgumentsFrame(StandardFrame* frame);

  void MaterializeStackLocals(Handle<JSObject> target,
                              Handle<ScopeInfo> scope_info);

  void MaterializeStackLocals(Handle<JSObject> target,
                              Handle<JSFunction> function);

  void UpdateStackLocalsFromMaterializedObject(Handle<JSObject> object,
                                               Handle<ScopeInfo> scope_info);

 private:
  bool ParameterIsShadowedByContextLocal(Handle<ScopeInfo> info,
                                         Handle<String> parameter_name);

  StandardFrame* frame_;
  FrameSummary frame_summary_;
  std::unique_ptr<DeoptimizedFrameInfo> deoptimized_frame_;
  std::unique_ptr<wasm::InterpretedFrame> wasm_interpreted_frame_;
  Isolate* isolate_;
  bool is_optimized_;
  bool is_interpreted_;
  bool is_bottommost_;
  bool has_adapted_arguments_;

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
