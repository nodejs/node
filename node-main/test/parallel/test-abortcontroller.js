// Flags: --expose-gc
'use strict';

const common = require('../common');
const { inspect } = require('util');

const assert = require('assert');

const {
  test,
  mock,
} = require('node:test');

const { setTimeout: sleep } = require('timers/promises');

// All of the the tests in this file depend on public-facing Node.js APIs.
// For tests that depend on Node.js internal APIs, please add them to
// test-abortcontroller-internal.js instead.

test('Abort is fired with the correct event type on AbortControllers', () => {
  // Tests that abort is fired with the correct event type on AbortControllers
  const ac = new AbortController();
  assert.ok(ac.signal);

  const fn = mock.fn(common.mustCall((event) => {
    assert.ok(event);
    assert.strictEqual(event.type, 'abort');
  }, 2));

  ac.signal.onabort = fn;
  ac.signal.addEventListener('abort', fn);

  ac.abort();
  ac.abort();
  assert.ok(ac.signal.aborted);

  assert.strictEqual(fn.mock.calls.length, 2);
});

test('Abort events are trusted', () => {
  // Tests that abort events are trusted
  const ac = new AbortController();

  const fn = mock.fn(common.mustCall((event) => {
    assert.ok(event.isTrusted);
  }));

  ac.signal.onabort = fn;
  ac.abort();
  assert.strictEqual(fn.mock.calls.length, 1);
});

test('Abort events have the same isTrusted reference', () => {
  // Tests that abort events have the same `isTrusted` reference
  const first = new AbortController();
  const second = new AbortController();
  let ev1, ev2;
  const ev3 = new Event('abort');

  first.signal.addEventListener('abort', (event) => {
    ev1 = event;
  });
  second.signal.addEventListener('abort', (event) => {
    ev2 = event;
  });
  first.abort();
  second.abort();
  const firstTrusted = Reflect.getOwnPropertyDescriptor(Object.getPrototypeOf(ev1), 'isTrusted').get;
  const secondTrusted = Reflect.getOwnPropertyDescriptor(Object.getPrototypeOf(ev2), 'isTrusted').get;
  const untrusted = Reflect.getOwnPropertyDescriptor(Object.getPrototypeOf(ev3), 'isTrusted').get;
  assert.strictEqual(firstTrusted, secondTrusted);
  assert.strictEqual(untrusted, firstTrusted);
});

test('AbortSignal is impossible to construct manually', () => {
  // Tests that AbortSignal is impossible to construct manually
  const ac = new AbortController();
  assert.throws(() => new ac.signal.constructor(), {
    code: 'ERR_ILLEGAL_CONSTRUCTOR',
  });
});

test('Symbol.toStringTag is correct', () => {
  // Symbol.toStringTag
  const toString = (o) => Object.prototype.toString.call(o);
  const ac = new AbortController();
  assert.strictEqual(toString(ac), '[object AbortController]');
  assert.strictEqual(toString(ac.signal), '[object AbortSignal]');
});

test('AbortSignal.abort() creates an already aborted signal', () => {
  const signal = AbortSignal.abort();
  assert.ok(signal.aborted);
});

test('AbortController properties and methods valiate the receiver', () => {
  const acSignalGet = Object.getOwnPropertyDescriptor(
    AbortController.prototype,
    'signal'
  ).get;
  const acAbort = AbortController.prototype.abort;

  const goodController = new AbortController();
  assert.ok(acSignalGet.call(goodController));
  acAbort.call(goodController);

  const badAbortControllers = [
    null,
    undefined,
    0,
    NaN,
    true,
    'AbortController',
    { __proto__: AbortController.prototype },
  ];
  for (const badController of badAbortControllers) {
    assert.throws(
      () => acSignalGet.call(badController),
      { name: 'TypeError' }
    );
    assert.throws(
      () => acAbort.call(badController),
      { name: 'TypeError' }
    );
  }
});

test('AbortSignal properties validate the receiver', () => {
  const signalAbortedGet = Object.getOwnPropertyDescriptor(
    AbortSignal.prototype,
    'aborted'
  ).get;

  const goodSignal = new AbortController().signal;
  assert.strictEqual(signalAbortedGet.call(goodSignal), false);

  const badAbortSignals = [
    null,
    undefined,
    0,
    NaN,
    true,
    'AbortSignal',
    { __proto__: AbortSignal.prototype },
  ];
  for (const badSignal of badAbortSignals) {
    assert.throws(
      () => signalAbortedGet.call(badSignal),
      { name: 'TypeError' }
    );
  }
});

test('AbortController inspection depth 1 or null works', () => {
  const ac = new AbortController();
  assert.strictEqual(inspect(ac, { depth: 1 }),
                     'AbortController { signal: [AbortSignal] }');
  assert.strictEqual(inspect(ac, { depth: null }),
                     'AbortController { signal: AbortSignal { aborted: false } }');
});

test('AbortSignal reason is set correctly', () => {
  // Test AbortSignal.reason
  const ac = new AbortController();
  ac.abort('reason');
  assert.strictEqual(ac.signal.reason, 'reason');
});

test('AbortSignal reasonable is set correctly with AbortSignal.abort()', () => {
  // Test AbortSignal.reason
  const signal = AbortSignal.abort('reason');
  assert.strictEqual(signal.reason, 'reason');
});

test('AbortSignal.timeout() works as expected', async () => {
  // Test AbortSignal timeout
  const signal = AbortSignal.timeout(10);
  assert.ok(!signal.aborted);

  const { promise, resolve } = Promise.withResolvers();

  const fn = mock.fn(common.mustCall(() => {
    assert.ok(signal.aborted);
    assert.strictEqual(signal.reason.name, 'TimeoutError');
    assert.strictEqual(signal.reason.code, 23);
    resolve();
  }));

  setTimeout(fn, 20);
  await promise;
});

test('AbortSignal.timeout() does not prevent the signal from being collected', async () => {
  // Test AbortSignal timeout doesn't prevent the signal
  // from being garbage collected.
  let ref;
  {
    ref = new globalThis.WeakRef(AbortSignal.timeout(1_200_000));
  }

  await sleep(10);
  globalThis.gc();
  assert.strictEqual(ref.deref(), undefined);
});

test('AbortSignal with a timeout is not collected while there is an active listener', async () => {
  // Test that an AbortSignal with a timeout is not gc'd while
  // there is an active listener on it.
  let ref;
  function handler() {}
  {
    ref = new globalThis.WeakRef(AbortSignal.timeout(1_200_000));
    ref.deref().addEventListener('abort', handler);
  }

  await sleep(10);
  globalThis.gc();
  assert.notStrictEqual(ref.deref(), undefined);
  assert.ok(ref.deref() instanceof AbortSignal);

  ref.deref().removeEventListener('abort', handler);

  await sleep(10);
  globalThis.gc();
  assert.strictEqual(ref.deref(), undefined);
});

test('Setting a long timeout should not keep the process open', () => {
  AbortSignal.timeout(1_200_000);
});

test('AbortSignal.reason should default', () => {
  // Test AbortSignal.reason default
  const signal = AbortSignal.abort();
  assert.ok(signal.reason instanceof DOMException);
  assert.strictEqual(signal.reason.code, 20);

  const ac = new AbortController();
  ac.abort();
  assert.ok(ac.signal.reason instanceof DOMException);
  assert.strictEqual(ac.signal.reason.code, 20);
});

test('abortSignal.throwIfAborted() works as expected', () => {
  // Test abortSignal.throwIfAborted()
  assert.throws(() => AbortSignal.abort().throwIfAborted(), {
    code: 20,
    name: 'AbortError',
  });

  // Does not throw because it's not aborted.
  const ac = new AbortController();
  ac.signal.throwIfAborted();
});

test('abortSignal.throwIfAobrted() works as expected (2)', () => {
  const originalDesc = Reflect.getOwnPropertyDescriptor(AbortSignal.prototype, 'aborted');
  const actualReason = new Error();
  Reflect.defineProperty(AbortSignal.prototype, 'aborted', { value: false });
  assert.throws(() => AbortSignal.abort(actualReason).throwIfAborted(), actualReason);
  Reflect.defineProperty(AbortSignal.prototype, 'aborted', originalDesc);
});

test('abortSignal.throwIfAobrted() works as expected (3)', () => {
  const originalDesc = Reflect.getOwnPropertyDescriptor(AbortSignal.prototype, 'reason');
  const actualReason = new Error();
  const fakeExcuse = new Error();
  Reflect.defineProperty(AbortSignal.prototype, 'reason', { value: fakeExcuse });
  assert.throws(() => AbortSignal.abort(actualReason).throwIfAborted(), actualReason);
  Reflect.defineProperty(AbortSignal.prototype, 'reason', originalDesc);
});
