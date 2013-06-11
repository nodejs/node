// Copyright 2013 the V8 project authors. All rights reserved.

// Check that we can traverse very deep stacks of ConsStrings using
// StringCharacterStram.  Check that Get(int) works on very deep stacks
// of ConsStrings.  These operations may not be very fast, but they
// should be possible without getting errors due to too deep recursion.

#include "v8.h"

#include "cctest.h"
#include "objects.h"

using namespace v8::internal;


TEST(Create) {
  CcTest::InitializeVM();
  Isolate* isolate = Isolate::Current();
  HandleScope scope(isolate);

  const int kNumSymbols = 30;
  Handle<Symbol> symbols[kNumSymbols];

  for (int i = 0; i < kNumSymbols; ++i) {
    symbols[i] = isolate->factory()->NewSymbol();
    CHECK(symbols[i]->IsName());
    CHECK(symbols[i]->IsSymbol());
    CHECK(symbols[i]->HasHashCode());
    CHECK_GT(symbols[i]->Hash(), 0);
    symbols[i]->ShortPrint();
    PrintF("\n");
#if OBJECT_PRINT
    symbols[i]->Print();
#endif
#if VERIFY_HEAP
    symbols[i]->Verify();
#endif
  }

  HEAP->PerformScavenge();
  HEAP->CollectAllGarbage(Heap::kNoGCFlags);

  // All symbols should be distinct.
  for (int i = 0; i < kNumSymbols; ++i) {
    CHECK(symbols[i]->SameValue(*symbols[i]));
    for (int j = i + 1; j < kNumSymbols; ++j) {
      CHECK(!symbols[i]->SameValue(*symbols[j]));
    }
  }
}
