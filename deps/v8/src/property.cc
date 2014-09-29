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
    visitor->VisitPointer(BitCast<Object**>(&current->holder_));
    visitor->VisitPointer(BitCast<Object**>(&current->transition_));
    current = current->next_;
  }
}


OStream& operator<<(OStream& os, const LookupResult& r) {
  if (!r.IsFound()) return os << "Not Found\n";

  os << "LookupResult:\n";
  os << " -cacheable = " << (r.IsCacheable() ? "true" : "false") << "\n";
  os << " -attributes = " << hex << r.GetAttributes() << dec << "\n";
  if (r.IsTransition()) {
    os << " -transition target:\n" << Brief(r.GetTransitionTarget()) << "\n";
  }
  switch (r.type()) {
    case NORMAL:
      return os << " -type = normal\n"
                << " -entry = " << r.GetDictionaryEntry() << "\n";
    case CONSTANT:
      return os << " -type = constant\n"
                << " -value:\n" << Brief(r.GetConstant()) << "\n";
    case FIELD:
      os << " -type = field\n"
         << " -index = " << r.GetFieldIndex().property_index() << "\n"
         << " -field type:";
      r.GetFieldType()->PrintTo(os);
      return os << "\n";
    case CALLBACKS:
      return os << " -type = call backs\n"
                << " -callback object:\n" << Brief(r.GetCallbackObject());
    case HANDLER:
      return os << " -type = lookup proxy\n";
    case INTERCEPTOR:
      return os << " -type = lookup interceptor\n";
    case NONEXISTENT:
      UNREACHABLE();
      break;
  }
  return os;
}


OStream& operator<<(OStream& os, const Descriptor& d) {
  return os << "Descriptor " << Brief(*d.GetKey()) << " @ "
            << Brief(*d.GetValue());
}

} }  // namespace v8::internal
