// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/property.h"

#include "src/handles-inl.h"
#include "src/ostreams.h"

namespace v8 {
namespace internal {

void LookupResult::Iterate(ObjectVisitor* visitor) {
  LookupResult* current = this;  // Could be NULL.
  while (current != NULL) {
    visitor->VisitPointer(bit_cast<Object**>(&current->holder_));
    visitor->VisitPointer(bit_cast<Object**>(&current->transition_));
    current = current->next_;
  }
}


OStream& operator<<(OStream& os, const LookupResult& r) {
  if (!r.IsFound()) return os << "Not Found\n";

  os << "LookupResult:\n";
  if (r.IsTransition()) {
    os << " -transition target:\n" << Brief(r.GetTransitionTarget()) << "\n";
  }
  return os;
}


OStream& operator<<(OStream& os, const Descriptor& d) {
  return os << "Descriptor " << Brief(*d.GetKey()) << " @ "
            << Brief(*d.GetValue());
}

} }  // namespace v8::internal
