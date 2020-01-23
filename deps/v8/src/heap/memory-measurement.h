// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MEMORY_MEASUREMENT_H_
#define V8_HEAP_MEMORY_MEASUREMENT_H_

#include "src/common/globals.h"
#include "src/objects/objects.h"

namespace v8 {
namespace internal {

class Heap;

class V8_EXPORT_PRIVATE MemoryMeasurement {
 public:
  explicit MemoryMeasurement(Isolate* isolate);
  Handle<JSPromise> EnqueueRequest(Handle<NativeContext> context,
                                   v8::MeasureMemoryMode mode);

 private:
  Isolate* isolate_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MEMORY_MEASUREMENT_H_
