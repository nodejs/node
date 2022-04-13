// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_JS_CPP_SNAPSHOT_H_
#define V8_HEAP_CPPGC_JS_CPP_SNAPSHOT_H_

#include "src/base/macros.h"

namespace v8 {

class Isolate;
class EmbedderGraph;

namespace internal {

class V8_EXPORT_PRIVATE CppGraphBuilder final {
 public:
  // Add the C++ snapshot to the existing |graph|. See CppGraphBuilderImpl for
  // algorithm internals.
  static void Run(v8::Isolate* isolate, v8::EmbedderGraph* graph, void* data);

  CppGraphBuilder() = delete;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_CPPGC_JS_CPP_SNAPSHOT_H_
