// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_JS_CPP_SNAPSHOT_H_
#define V8_HEAP_CPPGC_JS_CPP_SNAPSHOT_H_

#include "src/base/macros.h"
#include "src/profiler/heap-snapshot-common.h"

namespace v8 {
namespace internal {

class CppHeap;
class HeapSnapshotGenerator;

class V8_EXPORT_PRIVATE CppGraphBuilder final {
 public:
  // Add the C++ snapshot directly to the heap snapshot. See CppGraphBuilderImpl
  // for algorithm internals.
  static void Run(v8::internal::CppHeap* cpp_heap,
                  v8::internal::HeapSnapshotGenerator* generator,
                  CppHeapWrapperSet&& cpp_heap_wrappers);

  CppGraphBuilder() = delete;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_CPPGC_JS_CPP_SNAPSHOT_H_
