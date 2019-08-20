// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_JS_HEAP_COPY_REDUCER_H_
#define V8_COMPILER_JS_HEAP_COPY_REDUCER_H_

#include "src/compiler/graph-reducer.h"

namespace v8 {
namespace internal {
namespace compiler {

class JSHeapBroker;

// The heap copy reducer makes sure that the relevant heap data referenced
// by handles embedded in the graph is copied to the heap broker.
// TODO(jarin) This is just a temporary solution until the graph uses only
// ObjetRef-derived reference to refer to the heap data.
class V8_EXPORT_PRIVATE JSHeapCopyReducer : public Reducer {
 public:
  explicit JSHeapCopyReducer(JSHeapBroker* broker);

  const char* reducer_name() const override { return "JSHeapCopyReducer"; }

  Reduction Reduce(Node* node) override;

 private:
  JSHeapBroker* broker();

  JSHeapBroker* broker_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_JS_HEAP_COPY_REDUCER_H_
