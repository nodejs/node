// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --compile-hints-magic --parallel-compile-tasks-for-eager-toplevel

function outer() {
  class C {
    x = () => 1;
  }
  return new C();
}

//# allFunctionsCalledOnLoad
