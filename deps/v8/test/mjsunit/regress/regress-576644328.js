// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const b50 = 1n << (49n * 64n);
const a1049 = (1n << (1049n * 64n)) - 1n;
const a1048 = (1n << (1048n * 64n)) - 1n;
const y = (1n << (15n * 64n)) + 3n;
const z = (1n << (25n * 64n)) + 3n;
let sink;

function primeWorkEstimate() {
  try {
    class C extends 1n {}
  } catch {}
  for (let i = 0; i < 99; i++) {
    sink = (a1049 - BigInt(i)) / b50;
  }
  sink = a1048 / b50;
}

function checkNoSideEffectsToString() {
  let message;
  try {
    class C extends z {}
  } catch (e) {
    message = e.message;
  }
  assertEquals(
      `Class extends value ${z} is not a constructor or null`, message);
}

const callPrinterFn = eval(
    'let x; () => { if (false) x[1' + '0'.repeat(2000) +
    'n]; d8.terminate(); x(); }');

setTimeout(() => {
  checkNoSideEffectsToString();
  primeWorkEstimate();
  callPrinterFn();
  assertUnreachable();
}, 0);

primeWorkEstimate();

for (let i = 0; i < 99; i++) {
  sink = y % y;
}

d8.terminate();
sink = y % y;

checkNoSideEffectsToString();
assertUnreachable();
