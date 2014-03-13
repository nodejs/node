// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

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
      PrintF(out, " -index = %d", GetFieldIndex().field_index());
      PrintF(out, "\n");
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
    case TRANSITION:
      switch (GetTransitionDetails().type()) {
        case FIELD:
          PrintF(out, " -type = map transition\n");
          PrintF(out, " -map:\n");
          GetTransitionTarget()->Print(out);
          PrintF(out, "\n");
          return;
        case CONSTANT:
          PrintF(out, " -type = constant property transition\n");
          PrintF(out, " -map:\n");
          GetTransitionTarget()->Print(out);
          PrintF(out, "\n");
          return;
        case CALLBACKS:
          PrintF(out, " -type = callbacks transition\n");
          PrintF(out, " -callback object:\n");
          GetCallbackObject()->Print(out);
          return;
        default:
          UNREACHABLE();
          return;
      }
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
