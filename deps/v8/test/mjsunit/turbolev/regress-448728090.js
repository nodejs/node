// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax  --no-lazy-feedback-allocation

function wrap(f, permissive = true) {
  try { return f(); } catch (e) {}
}
const x = wrap(() => []);
function foo() {
  let [_] = wrap(() => x);
}
%PrepareFunctionForOptimization(foo);
const O = wrap(() => class extends foo {});
new O();
Object.defineProperty(x, undefined, {});
%OptimizeFunctionOnNextCall(foo);
foo();
