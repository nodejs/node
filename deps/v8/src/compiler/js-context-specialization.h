// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_CONTEXT_SPECIALIZATION_H_
#define V8_COMPILER_JS_CONTEXT_SPECIALIZATION_H_

#include "src/compiler/graph-reducer.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class JSGraph;
class JSOperatorBuilder;


// Specializes a given JSGraph to a given context, potentially constant folding
// some {LoadContext} nodes or strength reducing some {StoreContext} nodes.
class JSContextSpecializer : public AdvancedReducer {
 public:
  JSContextSpecializer(Editor* editor, JSGraph* jsgraph)
      : AdvancedReducer(editor), jsgraph_(jsgraph) {}

  Reduction Reduce(Node* node) override;

  // Visible for unit testing.
  Reduction ReduceJSLoadContext(Node* node);
  Reduction ReduceJSStoreContext(Node* node);

 private:
  Isolate* isolate() const;
  JSOperatorBuilder* javascript() const;
  JSGraph* jsgraph() const { return jsgraph_; }

  JSGraph* const jsgraph_;

  DISALLOW_COPY_AND_ASSIGN(JSContextSpecializer);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_CONTEXT_SPECIALIZATION_H_
