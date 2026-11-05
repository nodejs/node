// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MODULE
// Flags: --allow-natives-syntax

// 1. Exported `using` bindings are disposed in reverse declaration order at the
// end of synchronous module evaluation.
globalThis.syncLog = [];
const syncNs = await import(`data:text/javascript,
  using x = {
    val: 10,
    [Symbol.dispose]() { globalThis.syncLog.push('dispose x'); }
  };
  using y = {
    val: 20,
    [Symbol.dispose]() { globalThis.syncLog.push('dispose y'); }
  };
  globalThis.syncLog.push('sync body');
  export { x, y };
`);
assertEquals(['sync body', 'dispose y', 'dispose x'], globalThis.syncLog);
assertEquals(10, syncNs.x.val);
assertEquals(20, syncNs.y.val);

// 2. Exported `using` and `await using` bindings are disposed in reverse
// declaration order at the end of asynchronous module evaluation.
globalThis.asyncLog = [];
const asyncNs = await import(`data:text/javascript,
  using a = {
    val: 1,
    [Symbol.dispose]() { globalThis.asyncLog.push('dispose a'); }
  };
  await using b = {
    val: 2,
    async [Symbol.asyncDispose]() { globalThis.asyncLog.push('asyncDispose b'); }
  };
  using c = {
    val: 3,
    [Symbol.dispose]() { globalThis.asyncLog.push('dispose c'); }
  };
  globalThis.asyncLog.push('async body');
  export { a, b, c };
`);
assertEquals(
    ['async body', 'dispose c', 'asyncDispose b', 'dispose a'],
    globalThis.asyncLog);
assertEquals(1, asyncNs.a.val);
assertEquals(2, asyncNs.b.val);
assertEquals(3, asyncNs.c.val);

// 3. Exported `using` bindings are disposed when module evaluation throws.
globalThis.throwLog = [];
await assertThrowsAsync(
    import(`data:text/javascript,
      using x = {
        [Symbol.dispose]() { globalThis.throwLog.push('dispose x on throw'); }
      };
      throw new Error('boom');
      export { x };
    `),
    Error, 'boom');
assertEquals(['dispose x on throw'], globalThis.throwLog);

// 4. Non-disposable values in exported `using` and `await using` declarations
// reject with TypeError.
await assertThrowsAsync(
    import('data:text/javascript,using x = 42; export { x };'), TypeError);
await assertThrowsAsync(
    import('data:text/javascript,await using x = 42; export { x };'),
    TypeError);

// 5. Reassigning exported `using` or `await using` bindings rejects with
// TypeError.
await assertThrowsAsync(
    import('data:text/javascript,using x = null; x = 1; export { x };'),
    TypeError);
await assertThrowsAsync(
    import('data:text/javascript,await using x = null; x = 1; export { x };'),
    TypeError);

// 6. REPL_GLOBAL `using` and `await using` bindings forbid reassignment.
await assertThrowsAsync(
    %RuntimeEvaluateREPL('using repl_assign = null; repl_assign = 1;'),
    TypeError);
await assertThrowsAsync(
    %RuntimeEvaluateREPL(
        'await using repl_await_assign = null; repl_await_assign = 1;'),
    TypeError);

using topLevelExported = {
  value: 123,
  [Symbol.dispose]() {}
};
export { topLevelExported };
assertEquals(123, topLevelExported.value);
