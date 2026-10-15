// Flags: --experimental-stream-iter --expose-gc
'use strict';

const common = require('../common');
const assert = require('assert');
const { broadcast } = require('stream/iter');

async function detachConsumers(shared, count) {
  for (let i = 0; i < count; i++) {
    const iterator = shared.push()[Symbol.asyncIterator]();
    const pending = iterator.next();
    await iterator.return();
    await pending;
  }
}

async function testDetachedWaitersAreReleased() {
  const { broadcast: shared } = broadcast();

  await detachConsumers(shared, 100);
  global.gc();
  const before = process.memoryUsage().heapUsed;

  await detachConsumers(shared, 50_000);
  global.gc();
  const retained = process.memoryUsage().heapUsed - before;

  assert.ok(retained < 8 * 1024 * 1024,
            `Detached Broadcast waiters retained ${retained} bytes`);
  assert.strictEqual(shared.consumerCount, 0);
  shared.cancel();
}

// Consumers waiting for writes are notified once per write, across many
// writes: some wait with one next() at a time, one queues several next()
// calls, and one detaches while it is waiting. Every remaining consumer
// gets every chunk, in order.
async function testWaitersAcrossWrites() {
  // A 4-byte budget, so that the writer also waits for the consumers.
  const { writer, broadcast: shared } = broadcast({ budget: 4 });
  const n = 200;
  const readAll = async (iterator) => {
    const seen = [];
    for (;;) {
      const { done, value } = await iterator.next();
      if (done) return seen;
      for (const chunk of value) seen.push(chunk[0]);
    }
  };
  const readQueued = async (iterator) => {
    const seen = [];
    for (;;) {
      const results = await Promise.all(
        [iterator.next(), iterator.next(), iterator.next()]);
      for (const { done, value } of results) {
        if (done) return seen;
        for (const chunk of value) seen.push(chunk[0]);
      }
    }
  };
  const a = readAll(shared.push()[Symbol.asyncIterator]());
  const b = readAll(shared.push()[Symbol.asyncIterator]());
  const c = readQueued(shared.push()[Symbol.asyncIterator]());
  for (let i = 0; i < n; i++) {
    if (i === n / 2) {
      // Joins at the current position, waits, and detaches while waiting.
      const detaching = shared.push()[Symbol.asyncIterator]();
      const pending = detaching.next();
      await detaching.return();
      await pending;
    }
    await writer.write(new Uint8Array([i % 256]));
    if (i % 7 === 0) await new Promise(setImmediate);
  }
  await writer.end();
  const expected = Array.from({ length: n }, (_, i) => i % 256);
  assert.deepStrictEqual(await a, expected);
  assert.deepStrictEqual(await b, expected);
  assert.deepStrictEqual(await c, expected);
  assert.strictEqual(shared.consumerCount, 0);
}

testDetachedWaitersAreReleased()
  .then(testWaitersAcrossWrites)
  .then(common.mustCall());
