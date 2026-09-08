// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev
// Flags: --no-lazy-feedback-allocation --maglev-assert
// Flags: --max-turbolev-eager-inlined-bytecode-size=0

class C {
  method() {
    undefinedFunction();
  }
}

function foo(o) {
  try { o.method([]); } catch (e) {}
}
let c = new C();

%PrepareFunctionForOptimization(foo);
foo(c);
foo({});

%OptimizeFunctionOnNextCall(foo);
foo(c);
