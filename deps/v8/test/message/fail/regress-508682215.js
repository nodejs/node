// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --jit-fuzzing

let e = 0;
function a() {
  {
    let t;
    e();      // throw: e is not a function
    eval();   // marks `a` as may-eval (affects ScopeInfo)
  }
  function f() {   // declared but never called
    with (0) e;    // with-scope identifier lookup of outer const-binding
  }
}
a();
