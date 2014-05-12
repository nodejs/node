// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "property.h"

#include "handles-inl.h"

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


#ifdef OBJECT_PRINT
void LookupResult::Print(FILE* out) {
  if (!IsFound()) {
    PrintF(out, "Not Found\n");
    return;
  }

  PrintF(out, "LookupResult:\n");
  PrintF(out, " -cacheable = %s\n", IsCacheable() ? "true" : "false");
  PrintF(out, " -attributes = %x\n", GetAttributes());
  if (IsTransition()) {
    PrintF(out, " -transition target:\n");
    GetTransitionTarget()->Print(out);
    PrintF(out, "\n");
  }
  switch (type()) {
    case NORMAL:
      PrintF(out, " -type = normal\n");
      PrintF(out, " -entry = %d", GetDictionaryEntry());
      break;
    case CONSTANT:
      PrintF(out, " -type = constant\n");
      PrintF(out, " -value:\n");
      GetConstant()->Print(out);
      PrintF(out, "\n");
      break;
    case FIELD:
      PrintF(out, " -type = field\n");
      PrintF(out, " -index = %d\n", GetFieldIndex().field_index());
      PrintF(out, " -field type:\n");
      GetFieldType()->TypePrint(out);
      break;
    case CALLBACKS:
      PrintF(out, " -type = call backs\n");
      PrintF(out, " -callback object:\n");
      GetCallbackObject()->Print(out);
      break;
    case HANDLER:
      PrintF(out, " -type = lookup proxy\n");
      break;
    case INTERCEPTOR:
      PrintF(out, " -type = lookup interceptor\n");
      break;
    case NONEXISTENT:
      UNREACHABLE();
      break;
  }
}


void Descriptor::Print(FILE* out) {
  PrintF(out, "Descriptor ");
  GetKey()->ShortPrint(out);
  PrintF(out, " @ ");
  GetValue()->ShortPrint(out);
}
#endif

} }  // namespace v8::internal
