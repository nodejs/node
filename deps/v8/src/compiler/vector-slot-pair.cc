// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/vector-slot-pair.h"

#include "src/objects/feedback-vector.h"

namespace v8 {
namespace internal {

VectorSlotPair::VectorSlotPair() = default;

int VectorSlotPair::index() const {
  return vector_.is_null() ? -1 : FeedbackVector::GetIndex(slot_);
}

bool operator==(VectorSlotPair const& lhs, VectorSlotPair const& rhs) {
  return lhs.slot() == rhs.slot() &&
         lhs.vector().location() == rhs.vector().location() &&
         lhs.ic_state() == rhs.ic_state();
}

bool operator!=(VectorSlotPair const& lhs, VectorSlotPair const& rhs) {
  return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os, const VectorSlotPair& p) {
  if (p.IsValid()) {
    return os << "VectorSlotPair(" << p.slot() << ", "
              << InlineCacheState2String(p.ic_state()) << ")";
  }
  return os << "VectorSlotPair(INVALID)";
}

size_t hash_value(VectorSlotPair const& p) {
  return base::hash_combine(p.slot(), p.vector().location(), p.ic_state());
}

}  // namespace internal
}  // namespace v8
