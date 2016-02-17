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
class JSContextSpecialization final : public AdvancedReducer {
 public:
  JSContextSpecialization(Editor* editor, JSGraph* jsgraph,
                          MaybeHandle<Context> context)
      : AdvancedReducer(editor), jsgraph_(jsgraph), context_(context) {}

  Reduction Reduce(Node* node) final;

 private:
  Reduction ReduceJSLoadContext(Node* node);
  Reduction ReduceJSStoreContext(Node* node);

  // Returns the {Context} to specialize {node} to (if any).
  MaybeHandle<Context> GetSpecializationContext(Node* node);

  Isolate* isolate() const;
  JSOperatorBuilder* javascript() const;
  JSGraph* jsgraph() const { return jsgraph_; }
  MaybeHandle<Context> context() const { return context_; }

  JSGraph* const jsgraph_;
  MaybeHandle<Context> context_;

  DISALLOW_COPY_AND_ASSIGN(JSContextSpecialization);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_CONTEXT_SPECIALIZATION_H_
