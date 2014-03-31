// Copyright 2013 the V8 project authors. All rights reserved.
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

#if V8_TARGET_ARCH_ARM64

#include "globals.h"
#include "utils.h"
#include "arm64/decoder-arm64.h"


namespace v8 {
namespace internal {


void DispatchingDecoderVisitor::AppendVisitor(DecoderVisitor* new_visitor) {
  visitors_.remove(new_visitor);
  visitors_.push_front(new_visitor);
}


void DispatchingDecoderVisitor::PrependVisitor(DecoderVisitor* new_visitor) {
  visitors_.remove(new_visitor);
  visitors_.push_back(new_visitor);
}


void DispatchingDecoderVisitor::InsertVisitorBefore(
    DecoderVisitor* new_visitor, DecoderVisitor* registered_visitor) {
  visitors_.remove(new_visitor);
  std::list<DecoderVisitor*>::iterator it;
  for (it = visitors_.begin(); it != visitors_.end(); it++) {
    if (*it == registered_visitor) {
      visitors_.insert(it, new_visitor);
      return;
    }
  }
  // We reached the end of the list. The last element must be
  // registered_visitor.
  ASSERT(*it == registered_visitor);
  visitors_.insert(it, new_visitor);
}


void DispatchingDecoderVisitor::InsertVisitorAfter(
    DecoderVisitor* new_visitor, DecoderVisitor* registered_visitor) {
  visitors_.remove(new_visitor);
  std::list<DecoderVisitor*>::iterator it;
  for (it = visitors_.begin(); it != visitors_.end(); it++) {
    if (*it == registered_visitor) {
      it++;
      visitors_.insert(it, new_visitor);
      return;
    }
  }
  // We reached the end of the list. The last element must be
  // registered_visitor.
  ASSERT(*it == registered_visitor);
  visitors_.push_back(new_visitor);
}


void DispatchingDecoderVisitor::RemoveVisitor(DecoderVisitor* visitor) {
  visitors_.remove(visitor);
}


#define DEFINE_VISITOR_CALLERS(A)                                \
  void DispatchingDecoderVisitor::Visit##A(Instruction* instr) { \
    if (!(instr->Mask(A##FMask) == A##Fixed)) {                  \
      ASSERT(instr->Mask(A##FMask) == A##Fixed);                 \
    }                                                            \
    std::list<DecoderVisitor*>::iterator it;                     \
    for (it = visitors_.begin(); it != visitors_.end(); it++) {  \
      (*it)->Visit##A(instr);                                    \
    }                                                            \
  }
VISITOR_LIST(DEFINE_VISITOR_CALLERS)
#undef DEFINE_VISITOR_CALLERS


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_ARM64
