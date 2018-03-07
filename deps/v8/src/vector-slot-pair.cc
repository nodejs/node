// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/vector-slot-pair.h"

#include "src/feedback-vector.h"

namespace v8 {
namespace internal {

VectorSlotPair::VectorSlotPair() {}

int VectorSlotPair::index() const {
  return vector_.is_null() ? -1 : FeedbackVector::GetIndex(slot_);
}

bool operator==(VectorSlotPair const& lhs, VectorSlotPair const& rhs) {
  return lhs.slot() == rhs.slot() &&
         lhs.vector().location() == rhs.vector().location();
}

bool operator!=(VectorSlotPair const& lhs, VectorSlotPair const& rhs) {
  return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os, const VectorSlotPair& pair) {
  if (pair.IsValid()) {
    return os << "VectorSlotPair(" << pair.slot() << ")";
  }
  return os << "VectorSlotPair(INVALID)";
}

size_t hash_value(VectorSlotPair const& p) {
  return base::hash_combine(p.slot(), p.vector().location());
}

}  // namespace internal
}  // namespace v8
