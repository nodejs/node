// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VECTOR_SLOT_PAIR_H_
#define V8_VECTOR_SLOT_PAIR_H_

#include "src/globals.h"
#include "src/handles.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

class FeedbackVector;

// Defines a pair of {FeedbackVector} and {FeedbackSlot}, which
// is used to access the type feedback for a certain {Node}.
class V8_EXPORT_PRIVATE VectorSlotPair {
 public:
  VectorSlotPair();
  VectorSlotPair(Handle<FeedbackVector> vector, FeedbackSlot slot)
      : vector_(vector), slot_(slot) {}

  bool IsValid() const { return !vector_.is_null() && !slot_.IsInvalid(); }

  Handle<FeedbackVector> vector() const { return vector_; }
  FeedbackSlot slot() const { return slot_; }

  int index() const;

 private:
  Handle<FeedbackVector> vector_;
  FeedbackSlot slot_;
};

bool operator==(VectorSlotPair const&, VectorSlotPair const&);
bool operator!=(VectorSlotPair const&, VectorSlotPair const&);

std::ostream& operator<<(std::ostream& os, const VectorSlotPair& pair);

size_t hash_value(VectorSlotPair const&);

}  // namespace internal
}  // namespace v8

#endif  // V8_VECTOR_SLOT_PAIR_H_
