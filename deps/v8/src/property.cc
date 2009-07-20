// Copyright 2006-2008 the V8 project authors. All rights reserved.
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


#ifdef DEBUG
void LookupResult::Print() {
  if (!IsValid()) {
    PrintF("Not Found\n");
    return;
  }

  PrintF("LookupResult:\n");
  PrintF(" -cacheable = %s\n", IsCacheable() ? "true" : "false");
  PrintF(" -attributes = %x\n", GetAttributes());
  switch (type()) {
    case NORMAL:
      PrintF(" -type = normal\n");
      PrintF(" -entry = %d", GetDictionaryEntry());
      break;
    case MAP_TRANSITION:
      PrintF(" -type = map transition\n");
      PrintF(" -map:\n");
      GetTransitionMap()->Print();
      PrintF("\n");
      break;
    case CONSTANT_FUNCTION:
      PrintF(" -type = constant function\n");
      PrintF(" -function:\n");
      GetConstantFunction()->Print();
      PrintF("\n");
      break;
    case FIELD:
      PrintF(" -type = field\n");
      PrintF(" -index = %d", GetFieldIndex());
      PrintF("\n");
      break;
    case CALLBACKS:
      PrintF(" -type = call backs\n");
      PrintF(" -callback object:\n");
      GetCallbackObject()->Print();
      break;
    case INTERCEPTOR:
      PrintF(" -type = lookup interceptor\n");
      break;
    case CONSTANT_TRANSITION:
      PrintF(" -type = constant property transition\n");
      break;
    case NULL_DESCRIPTOR:
      PrintF(" =type = null descriptor\n");
      break;
  }
}


void Descriptor::Print() {
  PrintF("Descriptor ");
  GetKey()->ShortPrint();
  PrintF(" @ ");
  GetValue()->ShortPrint();
  PrintF(" %d\n", GetDetails().index());
}


#endif


} }  // namespace v8::internal
