// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_CONTEXT_RELAXATION_H_
#define V8_COMPILER_JS_CONTEXT_RELAXATION_H_

#include "src/compiler/graph-reducer.h"

namespace v8 {
namespace internal {
namespace compiler {

// Ensures that operations that only need to access the native context use the
// outer-most context rather than the specific context given by the AST graph
// builder. This makes it possible to use these operations with context
// specialization (e.g. for generating stubs) without forcing inner contexts to
// be embedded in generated code thus causing leaks and potentially using the
// wrong native context (i.e. stubs are shared between native contexts).
class JSContextRelaxation final : public Reducer {
 public:
  JSContextRelaxation() {}
  ~JSContextRelaxation() final {}

  Reduction Reduce(Node* node) final;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_CONTEXT_RELAXATION_H_
