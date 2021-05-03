// Flags: --no-warnings
'use strict';

const common = require('../common');
const { inspect } = require('util');

const { ok, strictEqual, throws } = require('assert');

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
  const firstTrusted = Reflect.getOwnPropertyDescriptor(ev1, 'isTrusted').get;
  const secondTrusted = Reflect.getOwnPropertyDescriptor(ev2, 'isTrusted').get;
  const untrusted = Reflect.getOwnPropertyDescriptor(ev3, 'isTrusted').get;
  strictEqual(firstTrusted, secondTrusted);
  strictEqual(untrusted, firstTrusted);
}

{
  // Tests that AbortSignal is impossible to construct manually
  const ac = new AbortController();
  throws(
    () => new ac.signal.constructor(),
    /^TypeError: Illegal constructor$/
  );
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
    Object.create(AbortController.prototype),
  ];
  for (const badController of badAbortControllers) {
    throws(
      () => acSignalGet.call(badController),
      { code: 'ERR_INVALID_THIS', name: 'TypeError' }
    );
    throws(
      () => acAbort.call(badController),
      { code: 'ERR_INVALID_THIS', name: 'TypeError' }
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
    Object.create(AbortSignal.prototype),
  ];
  for (const badSignal of badAbortSignals) {
    throws(
      () => signalAbortedGet.call(badSignal),
      { code: 'ERR_INVALID_THIS', name: 'TypeError' }
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
