// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_FRAME_SPECIALIZATION_H_
#define V8_COMPILER_JS_FRAME_SPECIALIZATION_H_

#include "src/compiler/graph-reducer.h"

namespace v8 {
namespace internal {

// Forward declarations.
class JavaScriptFrame;

namespace compiler {

// Forward declarations.
class JSGraph;


class JSFrameSpecialization final : public Reducer {
 public:
  JSFrameSpecialization(JavaScriptFrame const* frame, JSGraph* jsgraph)
      : frame_(frame), jsgraph_(jsgraph) {}
  ~JSFrameSpecialization() final {}

  Reduction Reduce(Node* node) final;

 private:
  Reduction ReduceOsrValue(Node* node);
  Reduction ReduceParameter(Node* node);

  Isolate* isolate() const;
  JavaScriptFrame const* frame() const { return frame_; }
  JSGraph* jsgraph() const { return jsgraph_; }

  JavaScriptFrame const* const frame_;
  JSGraph* const jsgraph_;

  DISALLOW_COPY_AND_ASSIGN(JSFrameSpecialization);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_FRAME_SPECIALIZATION_H_
