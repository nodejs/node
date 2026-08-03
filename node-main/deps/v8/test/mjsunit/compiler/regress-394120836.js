// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --js-staging

function f0() {}
function f1() { f0(); }
function f2() { f1(); }
function f3() { f1(); }

f1();
f2();
f2();
f3();
f3();
f1();
f3();

function wrap(f) { return f(); }

function inner(x) {
  const y = wrap(() => x.with());
  for (let i = 2; i < y.length; i++) {
    if (y instanceof Float16Array) {
      f1(y[i]);
    }
  }
}

try {
  (function test() {
    %PrepareFunctionForOptimization(inner);
    for (let ctor of [Int32Array, Float16Array, BigInt64Array]) {
      const ab = new ArrayBuffer(4 * ctor.BYTES_PER_ELEMENT);
      const ta = new ctor(ab);
      inner(ta);
      %OptimizeFunctionOnNextCall(inner);
    }
  })();
} catch (e) {}
