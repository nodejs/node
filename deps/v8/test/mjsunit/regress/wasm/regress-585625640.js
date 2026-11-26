// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

d8.file.execute('test/mjsunit/wasm/wasm-module-builder.js');

const builder = new WasmModuleBuilder();
builder.addFunction('id', kSig_i_i).addBody([kExprLocalGet, 0]).exportFunc();
const id = builder.instantiate().exports.id;

(function TestDeoptAndVerifier() {
  function f(x, a, obj, d) {
    x = x | 0;
    let y;
    if (0 < x) {
      a[0] = d;
      y = x;
    } else {
      a[1] = obj;
      y = (id(0) - x) | 0;
    }
    return y | 0;
  }

  const obj = {};
  %PrepareFunctionForOptimization(f);
  for (let i = 0; i < 5; i++) {
    f(1, [1.1, 2.2, 3.3, 4.4], obj, 0.5);
    f(0, [1.1, 2.2, 3.3, 4.4], obj, 0.5);
  }
  %OptimizeFunctionOnNextCall(f);

  const a = [1.1, 2.2, 3.3, 4.4];
  assertEquals(1, f(1, a, obj, 0.5));
  assertTrue(%HasDoubleElements(a));
  assertEquals(0.5, a[0]);
  assertEquals(2.2, a[1]);
})();

(function TestHoistedElementsTransition() {
  let leak0 = 0;
  let leak1 = 0;
  function f(x, a, b, o1, o2, d) {
    if (a.length < 4 || b.length < 4) return -1;
    let y, z0, z1;
    x = x | 0;
    if (0 < x) {
      z0 = a[0];
      z1 = a[1];
      b[0] = d;
      y = x;
    } else {
      a[0] = o1;
      a[1] = o2;
      b[2] = o1;
      y = (id(0) - x) | 0;
      z0 = 1337.7;
      z1 = 1338.8;
    }
    leak0 = z0;
    leak1 = z1;
    return y | 0;
  }

  const source = [1.2, 2.3, 3.4, 4.5];
  const makeArr = () => source.slice();
  const o1 = {};
  const o2 = {};

  const floats = new Float64Array(1);
  const ints = new Uint32Array(floats.buffer);
  ints[0] = 0x12345678;
  ints[1] = 0x42;
  const d = floats[0];

  %PrepareFunctionForOptimization(f);
  for (let i = 0; i < 10; i++) {
    f(i & 1 ? i : -i, makeArr(), makeArr(), o1, o2, d);
  }
  %OptimizeFunctionOnNextCall(f);

  const a = makeArr();
  const b = makeArr();
  assertEquals(1, f(1, a, b, o1, o2, d));
  assertTrue(%HasDoubleElements(a));
  assertTrue(%HasDoubleElements(b));
  assertEquals(1.2, a[0]);
  assertEquals(2.3, a[1]);
  assertEquals(d, b[0]);
  assertEquals(2.3, b[1]);
  assertEquals(3.4, b[2]);
  assertEquals(1.2, leak0);
  assertEquals(2.3, leak1);
})();
