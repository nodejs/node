// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNITTESTS_HEAP_UNIFIED_HEAP_UTILS_H_
#define V8_UNITTESTS_HEAP_UNIFIED_HEAP_UTILS_H_

#include "include/cppgc/heap.h"
#include "include/v8.h"
#include "test/unittests/heap/heap-utils.h"

namespace v8 {
namespace internal {

class CppHeap;

class UnifiedHeapTest : public TestWithHeapInternals {
 public:
  UnifiedHeapTest();
  ~UnifiedHeapTest() override;

  void CollectGarbageWithEmbedderStack();
  void CollectGarbageWithoutEmbedderStack();

  CppHeap& cpp_heap() const;
  cppgc::AllocationHandle& allocation_handle();

 private:
  bool saved_incremental_marking_wrappers_;
};

class WrapperHelper {
 public:
  // Sets up a V8 API object so that it points back to a C++ object. The setup
  // used is recognized by the GC and references will be followed for liveness
  // analysis (marking) as well as tooling (snapshot).
  static v8::Local<v8::Object> CreateWrapper(v8::Local<v8::Context> context,
                                             void* wrappable_object,
                                             const char* class_name = "");

  // Resets the connection of a wrapper (JS) to its wrappable (C++), meaning
  // that the wrappable object is not longer kept alive by the wrapper object.
  static void ResetWrappableConnection(v8::Local<v8::Object> api_object);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_UNITTESTS_HEAP_UNIFIED_HEAP_UTILS_H_
