// Flags: --no-warnings --expose-gc --expose-internals
'use strict';

const common = require('../common');
const { inspect } = require('util');

const {
  ok,
  strictEqual,
  throws,
} = require('assert');

const { aborted } = require('util');

const { setTimeout: sleep } = require('timers/promises');

{
  // Tests that abort is fired with the correct event type on AbortControllers
  const ac = new AbortController();
  ok(ac.signal);
  ac.signal.onabort = common.mustCall((event) => {
    ok(event);
    strictEqual(event.type, 'abort');
  });
  ac.signal.addEventListener('abort', common.mustCall((event) => {
    ok(event);
    strictEqual(event.type, 'abort');
  }), { once: true });
  ac.abort();
  ac.abort();
  ok(ac.signal.aborted);
}

{
  // Tests that abort events are trusted
  const ac = new AbortController();
  ac.signal.addEventListener('abort', common.mustCall((event) => {
    ok(event.isTrusted);
  }));
  ac.abort();
}

{
  // Tests that abort events have the same `isTrusted` reference
  const first = new AbortController();
  const second = new AbortController();
  let ev1, ev2;
  const ev3 = new Event('abort');
  first.signal.addEventListener('abort', common.mustCall((event) => {
    ev1 = event;
  }));
  second.signal.addEventListener('abort', common.mustCall((event) => {
    ev2 = event;
  }));
  first.abort();
  second.abort();
  const firstTrusted = Reflect.getOwnPropertyDescriptor(Object.getPrototypeOf(ev1), 'isTrusted').get;
  const secondTrusted = Reflect.getOwnPropertyDescriptor(Object.getPrototypeOf(ev2), 'isTrusted').get;
  const untrusted = Reflect.getOwnPropertyDescriptor(Object.getPrototypeOf(ev3), 'isTrusted').get;
  strictEqual(firstTrusted, secondTrusted);
  strictEqual(untrusted, firstTrusted);
}

{
  // Tests that AbortSignal is impossible to construct manually
  const ac = new AbortController();
  throws(() => new ac.signal.constructor(), {
    code: 'ERR_ILLEGAL_CONSTRUCTOR',
  });
}
{
  // Symbol.toStringTag
  const toString = (o) => Object.prototype.toString.call(o);
  const ac = new AbortController();
  strictEqual(toString(ac), '[object AbortController]');
  strictEqual(toString(ac.signal), '[object AbortSignal]');
}

{
  const signal = AbortSignal.abort();
  ok(signal.aborted);
}

{
  // Test that AbortController properties and methods validate the receiver
  const acSignalGet = Object.getOwnPropertyDescriptor(
    AbortController.prototype,
    'signal'
  ).get;
  const acAbort = AbortController.prototype.abort;

  const goodController = new AbortController();
  ok(acSignalGet.call(goodController));
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
    throws(
      () => acSignalGet.call(badController),
      { name: 'TypeError' }
    );
    throws(
      () => acAbort.call(badController),
      { name: 'TypeError' }
    );
  }
}

{
  // Test that AbortSignal properties validate the receiver
  const signalAbortedGet = Object.getOwnPropertyDescriptor(
    AbortSignal.prototype,
    'aborted'
  ).get;

  const goodSignal = new AbortController().signal;
  strictEqual(signalAbortedGet.call(goodSignal), false);

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
    throws(
      () => signalAbortedGet.call(badSignal),
      { name: 'TypeError' }
    );
  }
}

{
  const ac = new AbortController();
  strictEqual(inspect(ac, { depth: 1 }),
              'AbortController { signal: [AbortSignal] }');
  strictEqual(inspect(ac, { depth: null }),
              'AbortController { signal: AbortSignal { aborted: false } }');
}

{
  // Test AbortSignal.reason
  const ac = new AbortController();
  ac.abort('reason');
  strictEqual(ac.signal.reason, 'reason');
}

{
  // Test AbortSignal.reason
  const signal = AbortSignal.abort('reason');
  strictEqual(signal.reason, 'reason');
}

{
  // Test AbortSignal timeout
  const signal = AbortSignal.timeout(10);
  ok(!signal.aborted);
  setTimeout(common.mustCall(() => {
    ok(signal.aborted);
    strictEqual(signal.reason.name, 'TimeoutError');
    strictEqual(signal.reason.code, 23);
  }), 20);
}

{
  (async () => {
    // Test AbortSignal timeout can be GCed when no listeners exist
    let ref;
    {
      function lis() {

      }
      ref = new globalThis.WeakRef(AbortSignal.timeout(1_200_000));
      ref.deref().addEventListener('abort', lis);
      aborted(ref.deref(), {});

      await sleep(10);
      globalThis.gc();

      ref.deref().removeEventListener('abort', lis);
    }

    await sleep(10);
    globalThis.gc();
    strictEqual(ref.deref(), undefined);
  })().then(common.mustCall());
}

{
  // Test AbortSignal.reason default
  const signal = AbortSignal.abort();
  ok(signal.reason instanceof DOMException);
  strictEqual(signal.reason.code, 20);

  const ac = new AbortController();
  ac.abort();
  ok(ac.signal.reason instanceof DOMException);
  strictEqual(ac.signal.reason.code, 20);
}

{
  // Test abortSignal.throwIfAborted()
  throws(() => AbortSignal.abort().throwIfAborted(), {
    code: 20,
    name: 'AbortError',
  });

  // Does not throw because it's not aborted.
  const ac = new AbortController();
  ac.signal.throwIfAborted();
}

{
  const originalDesc = Reflect.getOwnPropertyDescriptor(AbortSignal.prototype, 'aborted');
  const actualReason = new Error();
  Reflect.defineProperty(AbortSignal.prototype, 'aborted', { value: false });
  throws(() => AbortSignal.abort(actualReason).throwIfAborted(), actualReason);
  Reflect.defineProperty(AbortSignal.prototype, 'aborted', originalDesc);
}

{
  const originalDesc = Reflect.getOwnPropertyDescriptor(AbortSignal.prototype, 'reason');
  const actualReason = new Error();
  const fakeExcuse = new Error();
  Reflect.defineProperty(AbortSignal.prototype, 'reason', { value: fakeExcuse });
  throws(() => AbortSignal.abort(actualReason).throwIfAborted(), actualReason);
  Reflect.defineProperty(AbortSignal.prototype, 'reason', originalDesc);
}
