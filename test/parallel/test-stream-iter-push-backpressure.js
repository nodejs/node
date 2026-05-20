// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const { push, text } = require('stream/iter');

async function testStrictBackpressure() {
  const { writer, readable } = push({
    highWaterMark: 1,
    backpressure: 'strict',
  });

  // First write should succeed synchronously
  assert.strictEqual(writer.writeSync('a'), true);
  // Second write should fail synchronously (buffer full)
  assert.strictEqual(writer.writeSync('b'), false);

  // Consume to free space, then end
  const resultPromise = text(readable);
  writer.end();
  const data = await resultPromise;
  assert.strictEqual(data, 'a');
}

async function testDropOldest() {
  const { writer, readable } = push({
    highWaterMark: 2,
    backpressure: 'drop-oldest',
  });

  assert.strictEqual(writer.writeSync('first'), true);
  assert.strictEqual(writer.writeSync('second'), true);
  // This should drop 'first' — return value is true (write accepted via drop)
  assert.strictEqual(writer.writeSync('third'), true);
  writer.end();

  const batches = [];
  for await (const batch of readable) {
    batches.push(batch);
  }
  // Should have 'second' and 'third'
  const allBytes = [];
  for (const batch of batches) {
    for (const chunk of batch) {
      allBytes.push(...chunk);
    }
  }
  const result = new TextDecoder().decode(new Uint8Array(allBytes));
  assert.strictEqual(result, 'secondthird');
}

async function testDropNewest() {
  const { writer, readable } = push({
    highWaterMark: 1,
    backpressure: 'drop-newest',
  });

  assert.strictEqual(writer.writeSync('kept'), true);
  // This is silently dropped — return value is true (accepted but discarded)
  assert.strictEqual(writer.writeSync('dropped'), true);
  writer.end();

  const data = await text(readable);
  assert.strictEqual(data, 'kept');
}

async function testBlockBackpressure() {
  const { writer, readable } = push({ highWaterMark: 1, backpressure: 'block' });

  // Fill the buffer
  writer.writeSync('a');

  // Next write should block (not throw, not drop)
  let writeState = 'pending';
  const writePromise = writer.write('b').then(() => { writeState = 'resolved'; });

  // The write cannot resolve until the buffer is drained, so a microtask
  // tick is sufficient to confirm it is still blocked.
  await new Promise(setImmediate);
  assert.strictEqual(writeState, 'pending'); // Still blocked

  // Read from the consumer to drain
  const iter = readable[Symbol.asyncIterator]();
  const first = await iter.next(); // Drains 'a'
  assert.strictEqual(first.done, false);

  // After draining, the pending write resolves as a microtask
  await new Promise(setImmediate);
  assert.strictEqual(writeState, 'resolved'); // Now unblocked

  writer.endSync();
  const second = await iter.next(); // Read 'b'
  assert.strictEqual(second.done, false);
  await writePromise;
}

async function testBlockWriteSyncEnqueues() {
  // With block policy, writeSync should enqueue the data even when the buffer
  // is full, returning false as a backpressure signal. The data IS accepted.
  const { writer, readable } = push({ highWaterMark: 1, backpressure: 'block' });

  // Fill the buffer
  assert.strictEqual(writer.writeSync('a'), true);

  // Buffer full: writeSync should enqueue and return false (data accepted)
  assert.strictEqual(writer.writeSync('b'), false);

  writer.endSync();

  // Both chunks should be delivered (drain flushes all slots into one batch)
  const result = await text(readable);
  assert.strictEqual(result, 'ab');
}

async function testStrictPendingQueueOverflow() {
  // With highWaterMark: 1 and strict, the pending writes queue is also limited to 1.
  // Filling the buffer (1 sync write) + filling the pending queue (1 async write)
  // should leave no room. A third write must reject with a RangeError.
  const { writer, readable } = push({
    highWaterMark: 1,
    backpressure: 'strict',
  });

  // Fill the buffer
  assert.strictEqual(writer.writeSync('a'), true);

  // This async write goes into the pending queue (buffer full, queue has room)
  const pendingWrite = writer.write('b');

  // This write should reject: buffer full AND pending queue at capacity
  await assert.rejects(
    () => writer.write('c'),
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
  testBlockWriteSyncEnqueues(),
  testStrictPendingQueueOverflow(),
]).then(common.mustCall());
