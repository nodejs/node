// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const { broadcast, text } = require('stream/iter');

// =============================================================================
// Basic broadcast
// =============================================================================

async function testBasicBroadcast() {
  const { writer, broadcast: bc } = broadcast();

  // Create two consumers
  const consumer1 = bc.push();
  const consumer2 = bc.push();

  assert.strictEqual(bc.consumerCount, 2);

  await writer.write('hello');
  await writer.end();

  const [data1, data2] = await Promise.all([
    text(consumer1),
    text(consumer2),
  ]);

  assert.strictEqual(data1, 'hello');
  assert.strictEqual(data2, 'hello');
}

async function testMultipleWrites() {
  const { writer, broadcast: bc } = broadcast({ highWaterMark: 10 });

  const consumer = bc.push();

  await writer.write('a');
  await writer.write('b');
  await writer.write('c');
  await writer.end();

  const data = await text(consumer);
  assert.strictEqual(data, 'abc');
}

async function testConsumerCount() {
  const { broadcast: bc } = broadcast();

  assert.strictEqual(bc.consumerCount, 0);

  const c1 = bc.push();
  assert.strictEqual(bc.consumerCount, 1);

  bc.push();
  assert.strictEqual(bc.consumerCount, 2);

  bc.cancel();

  // After cancel, consumer count drops to 0
  assert.strictEqual(bc.consumerCount, 0);

  // Consumers are detached and yield nothing
  const batches = [];
  for await (const batch of c1) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 0);
}

// =============================================================================
// Writer methods
// =============================================================================

async function testWriteSync() {
  const { writer, broadcast: bc } = broadcast({ highWaterMark: 2 });
  const consumer = bc.push();

  assert.strictEqual(writer.writeSync('a'), true);
  assert.strictEqual(writer.writeSync('b'), true);
  // Buffer full (highWaterMark=2, strict policy)
  assert.strictEqual(writer.writeSync('c'), false);

  writer.endSync();

  const data = await text(consumer);
  assert.strictEqual(data, 'ab');
}

async function testWritevSync() {
  const { writer, broadcast: bc } = broadcast({ highWaterMark: 10 });
  const consumer = bc.push();

  assert.strictEqual(writer.writevSync(['hello', ' ', 'world']), true);
  writer.endSync();

  const data = await text(consumer);
  assert.strictEqual(data, 'hello world');
}

async function testWriterEnd() {
  const { writer, broadcast: bc } = broadcast();
  const consumer = bc.push();

  await writer.write('data');
  const totalBytes = await writer.end();
  assert.strictEqual(totalBytes, 4); // 'data' = 4 UTF-8 bytes

  const data = await text(consumer);
  assert.strictEqual(data, 'data');
}

async function testWriterFail() {
  const { writer, broadcast: bc } = broadcast();
  const consumer = bc.push();

  writer.fail(new Error('test error'));

  await assert.rejects(
    async () => {
      // eslint-disable-next-line no-unused-vars
      for await (const _ of consumer) {
        assert.fail('Should not reach here');
      }
    },
    { message: 'test error' },
  );
}

// =============================================================================
// Cancel
// =============================================================================

async function testCancelWithoutReason() {
  const { broadcast: bc } = broadcast();
  const consumer = bc.push();

  bc.cancel();

  const batches = [];
  for await (const batch of consumer) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 0);
}

async function testCancelWithReason() {
  const { broadcast: bc } = broadcast();

  // Start a consumer that is waiting for data (promise pending)
  const consumer = bc.push();
  const resultPromise = text(consumer).catch((err) => err);

  // Give the consumer time to enter the waiting state
  await new Promise((resolve) => setImmediate(resolve));

  bc.cancel(new Error('cancelled'));

  const result = await resultPromise;
  assert.ok(result instanceof Error);
  assert.strictEqual(result.message, 'cancelled');
}

// =============================================================================
// Writer fail detaches consumers
// =============================================================================

async function testFailDetachesConsumers() {
  const { writer, broadcast: bc } = broadcast();
  const consumer1 = bc.push();
  const consumer2 = bc.push();

  assert.strictEqual(bc.consumerCount, 2);

  // Write some data, then fail the writer
  await writer.write('data');
  await writer.fail(new Error('writer failed'));

  // After fail, consumers are detached
  assert.strictEqual(bc.consumerCount, 0);

  // Both consumers should see the error
  await assert.rejects(
    async () => {
      // eslint-disable-next-line no-unused-vars
      for await (const _ of consumer1) {
        assert.fail('Should not reach here');
      }
    },
    { message: 'writer failed' },
  );

  await assert.rejects(
    async () => {
      // eslint-disable-next-line no-unused-vars
      for await (const _ of consumer2) {
        assert.fail('Should not reach here');
      }
    },
    { message: 'writer failed' },
  );
}

// =============================================================================
// Writer fail idempotent
// =============================================================================

async function testWriterFailIdempotent() {
  const { writer, broadcast: bc } = broadcast();
  const consumer = bc.push();
  writer.writeSync('hello');
  writer.fail(new Error('fail!'));
  // Second call is a no-op (already errored)
  writer.fail(new Error('fail2'));
  await assert.rejects(async () => {
    // eslint-disable-next-line no-unused-vars
    for await (const _ of consumer) { /* consume */ }
  }, { message: 'fail!' });
}

// cancel() with falsy reason (0, "", false) should still treat as error
async function testCancelWithFalsyReason() {
  const { broadcast: bc } = broadcast();
  const consumer = bc.push();
  const resultPromise = text(consumer).catch((err) => err);
  await new Promise((resolve) => setImmediate(resolve));
  bc.cancel(0);
  const result = await resultPromise;
  assert.strictEqual(result, 0);
}

// Late-joining consumer should read from oldest buffered entry
async function testLateJoinerSeesBufferedData() {
  const { writer, broadcast: bc } = broadcast({ highWaterMark: 16 });

  // Write data before any consumer joins
  writer.writeSync('before-join');
  writer.endSync();

  // Consumer joins after data is written
  const consumer = bc.push();
  const result = await text(consumer);
  assert.strictEqual(result, 'before-join');
}

Promise.all([
  testBasicBroadcast(),
  testMultipleWrites(),
  testConsumerCount(),
  testWriteSync(),
  testWritevSync(),
  testWriterEnd(),
  testWriterFail(),
  testCancelWithoutReason(),
  testCancelWithReason(),
  testCancelWithFalsyReason(),
  testFailDetachesConsumers(),
  testWriterFailIdempotent(),
  testLateJoinerSeesBufferedData(),
]).then(common.mustCall());
