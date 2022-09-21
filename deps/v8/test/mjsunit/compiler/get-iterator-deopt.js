// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function throwsRepeated(fn, ErrorType) {
  // Collect type feedback.
  %PrepareFunctionForOptimization(fn);
  for (let i = 0; i < 5; i++) assertThrows(fn, ErrorType);
  // Force compilation and run.
  %OptimizeFunctionOnNextCall(fn);
  assertThrows(fn, ErrorType);
  // If the function isn't optimized / turbofan tier not available,
  // a deopt happened on the call above.
  assertEquals(%IsTurbofanEnabled(), %ActiveTierIsTurbofan(fn));
}

function repeated(fn) {
  // Collect type feedback.
  %PrepareFunctionForOptimization(fn);
  for (let i = 0; i < 5; i++) fn();
  // Force compilation and run.
  %OptimizeFunctionOnNextCall(fn);
  fn();
  // If the function isn't optimized / turbofan tier not available,
  // a deopt happened on the call above.
  assertEquals(%IsTurbofanEnabled(), %ActiveTierIsTurbofan(fn));
}

repeated(() => { for (let p of "abc") { } });
repeated(() => { for (let p of [1, 2, 3]) { } });
throwsRepeated(() => { for (let p of {a: 1, b: 2}) { } }, TypeError);
let objWithIterator = { [Symbol.iterator]: function* () { yield 1; } };
repeated(() => { for (let p of objWithIterator) { } });
throwsRepeated(() => { for (let p of 5) { } }, TypeError);
throwsRepeated(() => { for (let p of new Number(5)) { } }, TypeError);
throwsRepeated(() => { for (let p of true) { } }, TypeError);
throwsRepeated(() => { for (let p of new BigInt(123)) { } }, TypeError);
throwsRepeated(() => { for (let p of new Symbol("symbol")) { } }, TypeError);
throwsRepeated(function testUndef() { for (let p of undefined) { } }, TypeError);
throwsRepeated(() => { for (let p of null) { } }, TypeError);
