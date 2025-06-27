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

#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/objects/name-inl.h"
#include "src/objects/objects.h"
#include "src/utils/ostreams.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

using SymbolsTest = TestWithIsolate;

TEST_F(SymbolsTest, Create) {
  const int kNumSymbols = 30;
  Handle<Symbol> symbols[kNumSymbols];

  StdoutStream os;
  for (int i = 0; i < kNumSymbols; ++i) {
    symbols[i] = isolate()->factory()->NewSymbol();
    CHECK(IsName(*symbols[i]));
    CHECK(IsSymbol(*symbols[i]));
    CHECK(symbols[i]->HasHashCode());
    CHECK(IsUniqueName(*symbols[i]));
    CHECK_GT(symbols[i]->hash(), 0u);
    os << Brief(*symbols[i]) << "\n";
#if OBJECT_PRINT
    Print(*symbols[i], os);
#endif
#if VERIFY_HEAP
    Object::ObjectVerify(*symbols[i], isolate());
#endif
  }

  InvokeMinorGC();
  InvokeMajorGC();

  // All symbols should be distinct.
  for (int i = 0; i < kNumSymbols; ++i) {
    CHECK(Object::SameValue(*symbols[i], *symbols[i]));
    for (int j = i + 1; j < kNumSymbols; ++j) {
      CHECK(!Object::SameValue(*symbols[i], *symbols[j]));
    }
  }
}

}  // namespace internal
}  // namespace v8
