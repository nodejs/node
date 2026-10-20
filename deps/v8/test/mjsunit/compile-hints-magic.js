// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

// Flags: --compile-hints-magic

function foo() {
  return 42;
}

//# allFunctionsCalledOnLoad

assertEquals(42, foo());
