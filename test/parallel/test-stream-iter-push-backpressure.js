// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const { push, text } = require('stream/iter');

async function testStrictBackpressure() {
  const kChunk = new Uint8Array(16384).fill(65); // 'A'
  const { writer, readable } = push({
    budget: 16384,
    backpressure: 'strict',
  });

  // First write fills the budget
  assert.strictEqual(writer.writeSync(kChunk), true);
  // Second write rejected (buffer full)
  assert.strictEqual(writer.writeSync(kChunk), false);

  // Consume to free space, then end
  const resultPromise = text(readable);
  writer.end();
  const data = await resultPromise;
  assert.strictEqual(data, 'A'.repeat(16384));
}

async function testDropOldest() {
  const chunk1 = new Uint8Array(16384).fill(49); // '1'
  const chunk2 = new Uint8Array(16384).fill(50); // '2'
  const chunk3 = new Uint8Array(16384).fill(51); // '3'
  const { writer, readable } = push({
    budget: 32768,
    backpressure: 'drop-oldest',
  });

  assert.strictEqual(writer.writeSync(chunk1), true);  // 16384 < 32768
  assert.strictEqual(writer.writeSync(chunk2), true);   // 32768 >= 32768
  // Buffer full: this drops chunk1, adds chunk3
  assert.strictEqual(writer.writeSync(chunk3), true);
  writer.end();

  const data = await text(readable);
  // chunk2 ('2' x 16384) + chunk3 ('3' x 16384) survived
  assert.strictEqual(data, '2'.repeat(16384) + '3'.repeat(16384));
}

async function testDropNewest() {
  const kept = new Uint8Array(16384).fill(75);    // 'K'
  const dropped = new Uint8Array(16384).fill(68); // 'D'
  const { writer, readable } = push({
    budget: 16384,
    backpressure: 'drop-newest',
  });

  assert.strictEqual(writer.writeSync(kept), true);
  // Buffer full: new write is silently discarded
  assert.strictEqual(writer.writeSync(dropped), true);
  writer.end();

  const data = await text(readable);
  assert.strictEqual(data, 'K'.repeat(16384));
}

async function testBlockBackpressure() {
  const kChunk = new Uint8Array(16384);
  const { writer, readable } = push({ budget: 16384, backpressure: 'unbounded' });

  // Fill the buffer
  writer.writeSync(kChunk);

  // Next write should block (not throw, not drop)
  let writeState = 'pending';
  const writePromise = writer.write(kChunk).then(() => { writeState = 'resolved'; });

  // The write cannot resolve until the buffer is drained, so a microtask
  // tick is sufficient to confirm it is still blocked.
  await new Promise(setImmediate);
  assert.strictEqual(writeState, 'pending'); // Still blocked

  // Read from the consumer to drain
  const iter = readable[Symbol.asyncIterator]();
  const first = await iter.next();
  assert.strictEqual(first.done, false);

  // After draining, the pending write resolves as a microtask
  await new Promise(setImmediate);
  assert.strictEqual(writeState, 'resolved'); // Now unblocked

  writer.endSync();
  const second = await iter.next();
  assert.strictEqual(second.done, false);
  await writePromise;
}

async function testBlockWriteSyncDoesNotEnqueue() {
  // With block policy, writeSync returns false when the buffer is full.
  // The data is NOT accepted — writeSync only operates on the slots buffer.
  // The caller should fall back to write() which uses the pending queue.
  const kChunk = new Uint8Array(16384);
  const { writer, readable } = push({ budget: 16384, backpressure: 'unbounded' });

  // Fill the buffer
  assert.strictEqual(writer.writeSync(kChunk), true);

  // Buffer full: writeSync returns false, data NOT enqueued
  assert.strictEqual(writer.writeSync(kChunk), false);

  // Use the async write (try-fallback pattern) — this goes to pending queue
  const consuming = text(readable);
  await writer.write(kChunk);
  await writer.end();

  // Both chunks should be delivered (sync + async)
  const result = await consuming;
  assert.strictEqual(result.length, 32768);
}

async function testStrictPendingQueueOverflow() {
  // With strict mode, the pending writes queue is limited to 1.
  // Filling the buffer (1 sync write) + filling the pending queue (1 async write)
  // should leave no room. A third write must reject with a RangeError.
  const kChunk = new Uint8Array(16384);
  const { writer, readable } = push({
    budget: 16384,
    backpressure: 'strict',
  });

  // Fill the buffer
  assert.strictEqual(writer.writeSync(kChunk), true);

  // This async write goes into the pending queue (buffer full, queue has room)
  const pendingWrite = writer.write(kChunk);

  // This write should reject: buffer full AND pending queue at capacity
  await assert.rejects(
    () => writer.write(kChunk),
    {
      code: 'ERR_INVALID_STATE',
      name: 'RangeError',
    },
  );

  // Clean up: drain the readable
  const iter = readable[Symbol.asyncIterator]();
  await iter.next();
  await iter.next();
  await pendingWrite;
  writer.endSync();
  await iter.return();
}

Promise.all([
  testStrictBackpressure(),
  testDropOldest(),
  testDropNewest(),
  testBlockBackpressure(),
  testBlockWriteSyncDoesNotEnqueue(),
  testStrictPendingQueueOverflow(),
]).then(common.mustCall());
