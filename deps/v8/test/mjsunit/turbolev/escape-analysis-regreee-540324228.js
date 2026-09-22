// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis

function bar() {
  throw 1;
}
%NeverOptimizeFunction(bar);

function foo() {
  const o = { x: 1 };
  while (true) {
    o.x = 42;
    bar();
  }
}

%PrepareFunctionForOptimization(foo);
try { foo(); } catch(e) {}
try { foo(); } catch(e) {}

%OptimizeFunctionOnNextCall(foo);
try { foo(); } catch(e) {}
