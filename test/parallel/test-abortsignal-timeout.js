// Flags: --no-warnings --expose-gc --expose-internals
'use strict';

require('../common');
const { aborted } = require('util');

const {
  ok,
  notStrictEqual,
  strictEqual,
} = require('assert');

const {
  kWeakHandler,
} = require('internal/event_target');

const { setTimeout: sleep } = require('timers/promises');
const { test } = require('node:test');

test('Test AbortSignal timeout', async () => {
  // Test AbortSignal timeout
  const signal = AbortSignal.timeout(10);
  ok(!signal.aborted);
  await sleep(20);
  ok(signal.aborted);
  strictEqual(signal.reason.name, 'TimeoutError');
  strictEqual(signal.reason.code, 23);
});

test("AbortSignal timeout doesn't prevent the signal from being garbage collected", async () => {
  let ref;
  {
    ref = new globalThis.WeakRef(AbortSignal.timeout(1_200_000));
  }

  await sleep(10);
  globalThis.gc();
  strictEqual(ref.deref(), undefined);
});

test("AbortSignal with a timeout is not gc'd while there is an active listener on it", async () => {
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
  notStrictEqual(ref.deref(), undefined);
  ok(ref.deref() instanceof AbortSignal);

  ref.deref().removeEventListener('abort', handler);

  await sleep(10);
  globalThis.gc();
  strictEqual(ref.deref(), undefined);
});

test("AbortSignal.timeout should be gc'd when there is only a weak listener active on it", async () => {
  let ref;
  function handler() {}
  {
    ref = new globalThis.WeakRef(AbortSignal.timeout(1_200_000));
    ref.deref().addEventListener('abort', handler, { [kWeakHandler]: {} });
  }

  await sleep(10);
  globalThis.gc();
  strictEqual(ref.deref(), undefined);
});

test('should not keep the process open', () => {
  // Setting a long timeout (20 minutes here) should not
  // keep the Node.js process open (the timer is unref'd)
  AbortSignal.timeout(1_200_000);
});

test('AbortSignal.timeout should not leak memory when using weak and regular listeners', async () => {
  function getMemoryAllocatedInMB() {
    return Math.round(process.memoryUsage().rss / 1024 / 1024 * 100) / 100;
  }

  async function createALotOfAbortSignals() {
    for (let i = 0; i < 10000; i++) {
      function lis() {

      }

      const timeoutSignal = AbortSignal.timeout(1_000_000_000);
      timeoutSignal.addEventListener('abort', lis);
      aborted(timeoutSignal, {});
      timeoutSignal.removeEventListener('abort', lis);
    }

    await sleep(10);
    globalThis.gc();
  }

  // Making sure we create some data so we won't catch something that is related to the infra
  await createALotOfAbortSignals();

  const currentMemory = getMemoryAllocatedInMB();

  await createALotOfAbortSignals();

  const newMemory = getMemoryAllocatedInMB();

  strictEqual(newMemory - currentMemory < 100, true, new Error(`After consuming 10 items the memory increased by ${Math.floor(newMemory - currentMemory)}MB`));
});
