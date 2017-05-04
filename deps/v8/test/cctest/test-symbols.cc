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

// Check that we can traverse very deep stacks of ConsStrings using
// StringCharacterStram.  Check that Get(int) works on very deep stacks
// of ConsStrings.  These operations may not be very fast, but they
// should be possible without getting errors due to too deep recursion.

#include "src/factory.h"
#include "src/isolate.h"
#include "src/objects.h"
#include "src/ostreams.h"
// FIXME(mstarzinger, marja): This is weird, but required because of the missing
// (disallowed) include: src/factory.h -> src/objects-inl.h
#include "src/objects-inl.h"
// FIXME(mstarzinger, marja): This is weird, but required because of the missing
// (disallowed) include: src/feedback-vector.h ->
// src/feedback-vector-inl.h
#include "src/feedback-vector-inl.h"
#include "src/v8.h"
#include "test/cctest/cctest.h"

using namespace v8::internal;


TEST(Create) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  const int kNumSymbols = 30;
  Handle<Symbol> symbols[kNumSymbols];

  OFStream os(stdout);
  for (int i = 0; i < kNumSymbols; ++i) {
    symbols[i] = isolate->factory()->NewSymbol();
    CHECK(symbols[i]->IsName());
    CHECK(symbols[i]->IsSymbol());
    CHECK(symbols[i]->HasHashCode());
    CHECK_GT(symbols[i]->Hash(), 0u);
    os << Brief(*symbols[i]) << "\n";
#if OBJECT_PRINT
    symbols[i]->Print(os);
#endif
#if VERIFY_HEAP
    symbols[i]->ObjectVerify();
#endif
  }

  CcTest::CollectGarbage(i::NEW_SPACE);
  CcTest::CollectAllGarbage();

  // All symbols should be distinct.
  for (int i = 0; i < kNumSymbols; ++i) {
    CHECK(symbols[i]->SameValue(*symbols[i]));
    for (int j = i + 1; j < kNumSymbols; ++j) {
      CHECK(!symbols[i]->SameValue(*symbols[j]));
    }
  }
}
