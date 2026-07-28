// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --js-immutable-arraybuffer

(function TestSliceToImmutableMaglev() {
  function store(arr, val) {
    arr[0] = val;
  }

  let ab = new ArrayBuffer(16);
  let ta = new Uint8Array(ab);

  %PrepareFunctionForOptimization(store);
  store(ta, 10);
  store(ta, 20);
  %OptimizeFunctionOnNextCall(store);
  store(ta, 30);

  let immAb = ab.sliceToImmutable();
  let ta_imm = new Uint8Array(immAb);

  assertTrue(ta_imm.buffer.immutable);

  let before = ta_imm[0];
  // This should not write to the buffer.
  // In strict mode it throws, in sloppy it silently fails.
  store(ta_imm, 0xAA);
  let after = ta_imm[0];

  assertNotEquals(0xAA, after);
})();

(function TestTransferToImmutableMaglev() {
  function store(arr, val) {
    arr[0] = val;
  }

  let ab = new ArrayBuffer(16);
  let ta = new Uint8Array(ab);

  %PrepareFunctionForOptimization(store);
  store(ta, 10);
  store(ta, 20);
  %OptimizeFunctionOnNextCall(store);
  store(ta, 30);

  let immAb = ab.transferToImmutable();
  let ta_imm = new Uint8Array(immAb);

  assertTrue(ta_imm.buffer.immutable);

  let before = ta_imm[0];
  store(ta_imm, 0xBB);
  let after = ta_imm[0];

  assertNotEquals(0xBB, after);
})();

(function TestSliceToImmutableTurbofan() {
  function store(arr, val) {
    arr[0] = val;
  }

  let ab = new ArrayBuffer(16);
  let ta = new Uint8Array(ab);

  %PrepareFunctionForOptimization(store);
  store(ta, 10);
  store(ta, 20);
  %OptimizeFunctionOnNextCall(store);
  store(ta, 30);

  let immAb = ab.sliceToImmutable();
  let ta_imm = new Uint8Array(immAb);

  assertTrue(ta_imm.buffer.immutable);

  let before = ta_imm[0];
  store(ta_imm, 0xCC);
  let after = ta_imm[0];

  assertNotEquals(0xCC, after);
})();
