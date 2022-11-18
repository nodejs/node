// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function throwsRepeated(fn, ErrorType, required_compilation_count) {
  for (let j = 0; j < (required_compilation_count ?? 1); j++) {
    // Collect type feedback.
    %PrepareFunctionForOptimization(fn);
    for (let i = 0; i < 5; i++) assertThrows(fn, ErrorType);
    // Force compilation and run.
    %OptimizeFunctionOnNextCall(fn);
    assertThrows(fn, ErrorType);
  }
  // If the function isn't optimized / turbofan tier not available,
  // a deopt happened on the call above.
  assertEquals(%IsTurbofanEnabled(), %ActiveTierIsTurbofan(fn));
}

function repeated(fn) {
  // Collect type feedback.
  %PrepareFunctionForOptimization(fn);
  for (let i = 0; i < 2; i++) fn();
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
throwsRepeated(() => { for (let p of Symbol("symbol")) { } }, TypeError);
throwsRepeated(() => { for (let p of undefined) { } }, TypeError);
throwsRepeated(() => { for (let p of null) { } }, TypeError);

throwsRepeated(() => (undefined).val = undefined, TypeError);
throwsRepeated(() => (undefined)["test"] = undefined, TypeError);
throwsRepeated(() => (undefined)[Symbol("test")] = undefined, TypeError);
throwsRepeated(() => (undefined)[null] = undefined, TypeError);
throwsRepeated(() => (undefined)[undefined] = undefined, TypeError);
throwsRepeated(() => (undefined)[0] = undefined, TypeError);
throwsRepeated(() => (undefined)[NaN] = undefined, TypeError);
throwsRepeated(() => (null)[0] = undefined, TypeError);
// BigInt.asIntN() deopts once but provides a better suitable compile result
// on the second compilation which doesn't deopt any more.
let compiles = 2;
throwsRepeated(() => BigInt.asIntN(2, 2), TypeError, compiles);
throwsRepeated(() => BigInt.asIntN(2, () => {}), SyntaxError, compiles);
throwsRepeated(() => BigInt.asIntN(2, {some: Object}), SyntaxError, compiles);
throwsRepeated(() => BigInt.asIntN(2, Symbol("test")), TypeError, compiles);
throwsRepeated(() => BigInt.asIntN(2, null), TypeError, compiles);
