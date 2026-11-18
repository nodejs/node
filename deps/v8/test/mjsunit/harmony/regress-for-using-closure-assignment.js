// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testLazyUsingUnusedClosure() {
  for (using x = null; ;) {
    () => {};
    x = 1;
  }
}
assertThrows(
    testLazyUsingUnusedClosure, TypeError, 'Assignment to using variable.');

function testLazyUsingCapturingClosure() {
  for (using x = null; ;) {
    () => x;
    x = 1;
  }
}
assertThrows(
    testLazyUsingCapturingClosure, TypeError, 'Assignment to using variable.');

assertThrows(() => {
  for (using x = null; ;) {
    () => {};
    x = 1;
  }
}, TypeError, 'Assignment to using variable.');

assertThrows(() => {
  for (using x = { [Symbol.dispose]() {} }; ;) {
    x = 1;
  }
}, TypeError, 'Assignment to using variable.');

async function testLazyAwaitUsingUnusedClosure() {
  for (await using x = null; ;) {
    () => {};
    x = 1;
  }
}

async function testLazyAwaitUsingCapturingClosure() {
  for (await using x = null; ;) {
    () => x;
    x = 1;
  }
}

function testLazyConstCapturingClosure() {
  for (const x = 1; ;) {
    () => x;
    x = 2;
  }
}
assertThrows(
    testLazyConstCapturingClosure, TypeError, 'Assignment to constant variable.');

assertThrowsAsync(
    testLazyAwaitUsingUnusedClosure(), TypeError,
    'Assignment to await using variable.');
assertThrowsAsync(
    testLazyAwaitUsingCapturingClosure(), TypeError,
    'Assignment to await using variable.');
assertThrowsAsync(
    (async () => {
      for (await using x = null; ;) {
        () => {};
        x = 1;
      }
    })(),
    TypeError, 'Assignment to await using variable.');
