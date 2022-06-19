// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_REFERENCE_SUMMARIZER_H_
#define V8_HEAP_REFERENCE_SUMMARIZER_H_

#include <unordered_set>

#include "src/objects/heap-object.h"

namespace v8 {
namespace internal {

class Heap;

class ReferenceSummary {
 public:
  ReferenceSummary() = default;
  ReferenceSummary(ReferenceSummary&& other) V8_NOEXCEPT
      : strong_references_(std::move(other.strong_references_)),
        weak_references_(std::move(other.weak_references_)) {}

  // Produces a set of objects referred to by the object. This function uses a
  // realistic marking visitor, so its results are likely to match real GC
  // behavior. Intended only for verification.
  static ReferenceSummary SummarizeReferencesFrom(Heap* heap, HeapObject obj);

  // All objects which the chosen object has strong pointers to.
  std::unordered_set<HeapObject, Object::Hasher>& strong_references() {
    return strong_references_;
  }

  // All objects which the chosen object has weak pointers to. The values in
  // ephemeron hash tables are also included here, even though they aren't
  // normal weak pointers.
  std::unordered_set<HeapObject, Object::Hasher>& weak_references() {
    return weak_references_;
  }

  void Clear() {
    strong_references_.clear();
    weak_references_.clear();
  }

 private:
  std::unordered_set<HeapObject, Object::Hasher> strong_references_;
  std::unordered_set<HeapObject, Object::Hasher> weak_references_;
  DISALLOW_GARBAGE_COLLECTION(no_gc)
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_REFERENCE_SUMMARIZER_H_
