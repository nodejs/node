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

testDetachedWaitersAreReleased().then(common.mustCall());
