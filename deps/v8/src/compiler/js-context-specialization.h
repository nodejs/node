// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_CONTEXT_SPECIALIZATION_H_
#define V8_COMPILER_JS_CONTEXT_SPECIALIZATION_H_

#include "src/compiler/graph-reducer.h"
#include "src/compiler/js-graph.h"
#include "src/contexts.h"

namespace v8 {
namespace internal {
namespace compiler {

// Specializes a given JSGraph to a given context, potentially constant folding
// some {LoadContext} nodes or strength reducing some {StoreContext} nodes.
class JSContextSpecializer : public Reducer {
 public:
  explicit JSContextSpecializer(JSGraph* jsgraph) : jsgraph_(jsgraph) {}

  Reduction Reduce(Node* node) override;

  // Visible for unit testing.
  Reduction ReduceJSLoadContext(Node* node);
  Reduction ReduceJSStoreContext(Node* node);

 private:
  JSGraph* jsgraph_;
};
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_JS_CONTEXT_SPECIALIZATION_H_
