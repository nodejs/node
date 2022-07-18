// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_PROMOTE_YOUNG_GENERATION_H_
#define V8_HEAP_PROMOTE_YOUNG_GENERATION_H_

namespace v8 {
namespace internal {

class Heap;

/**
 * `PromoteYoungGenerationGC` is a special GC mode used in fast promotion mode
 * to quickly promote all objects in new space to old space, thus evacuating all
 * of new space and leaving it empty.
 */
class PromoteYoungGenerationGC {
 public:
  explicit PromoteYoungGenerationGC(Heap* heap) : heap_(heap) {}
  void EvacuateYoungGeneration();

 private:
  Heap* const heap_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_PROMOTE_YOUNG_GENERATION_H_
