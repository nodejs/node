// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --enable-queue-microtask

(function TestQueueMicrotask() {
  assertDoesNotThrow(() => queueMicrotask(() => {}));
})();

(function TestQueueMicrotaskNotAFunction() {
  assertThrows(() => queueMicrotask(), TypeError);
  assertThrows(() => queueMicrotask(undefined), TypeError);
  assertThrows(() => queueMicrotask(null), TypeError);
  assertThrows(() => queueMicrotask(1), TypeError);
  assertThrows(() => queueMicrotask('1'), TypeError);
  assertThrows(() => queueMicrotask({}), TypeError);
  assertThrows(() => queueMicrotask([]), TypeError);
  assertThrows(() => queueMicrotask(Symbol()), TypeError);
})();

(function TestQueueMicrotaskOrder() {
  let result = '';
  queueMicrotask(() => result += 'a');
  result += 'b';
  Promise.resolve().then(() => result += 'c');

  %PerformMicrotaskCheckpoint();
  assertEquals('bac', result);
})();

(function TestQueueMicrotaskNested() {
  let result = '';
  queueMicrotask(() => {
    result += 'a';
    queueMicrotask(() => result += 'c');
  });
  result += 'b';

  %PerformMicrotaskCheckpoint();
  assertEquals('bac', result);
})();
