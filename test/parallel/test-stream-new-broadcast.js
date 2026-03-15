'use strict';

const common = require('../common');
const assert = require('assert');
const { broadcast, Broadcast, from, text } = require('stream/new');

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

  // Consume c1 to completion (it returns immediately since no data has been
  // pushed and we haven't ended yet - but we'll cancel to detach)
  bc.cancel();

  // After cancel, consumers are detached
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
  assert.ok(totalBytes > 0);

  const data = await text(consumer);
  assert.strictEqual(data, 'data');
}

async function testWriterFail() {
  const { writer, broadcast: bc } = broadcast();
  const consumer = bc.push();

  await writer.fail(new Error('test error'));

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
// Backpressure policies
// =============================================================================

async function testDropOldest() {
  const { writer, broadcast: bc } = broadcast({
    highWaterMark: 2,
    backpressure: 'drop-oldest',
  });
  const consumer = bc.push();

  writer.writeSync('first');
  writer.writeSync('second');
  // This should drop 'first'
  writer.writeSync('third');
  writer.endSync();

  const data = await text(consumer);
  assert.strictEqual(data, 'secondthird');
}

async function testDropNewest() {
  const { writer, broadcast: bc } = broadcast({
    highWaterMark: 1,
    backpressure: 'drop-newest',
  });
  const consumer = bc.push();

  writer.writeSync('kept');
  // This should be silently dropped
  writer.writeSync('dropped');
  writer.endSync();

  const data = await text(consumer);
  assert.strictEqual(data, 'kept');
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
// Broadcast.from
// =============================================================================

async function testBroadcastFromAsyncIterable() {
  const source = from('broadcast-from');
  const { broadcast: bc } = Broadcast.from(source);
  const consumer = bc.push();

  const data = await text(consumer);
  assert.strictEqual(data, 'broadcast-from');
}

async function testBroadcastFromMultipleConsumers() {
  const source = from('shared-data');
  const { broadcast: bc } = Broadcast.from(source);

  const c1 = bc.push();
  const c2 = bc.push();

  const [data1, data2] = await Promise.all([
    text(c1),
    text(c2),
  ]);

  assert.strictEqual(data1, 'shared-data');
  assert.strictEqual(data2, 'shared-data');
}

// =============================================================================
// AbortSignal
// =============================================================================

async function testAbortSignal() {
  const ac = new AbortController();
  const { broadcast: bc } = broadcast({ signal: ac.signal });
  const consumer = bc.push();

  ac.abort();

  const batches = [];
  for await (const batch of consumer) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 0);
}

async function testAlreadyAbortedSignal() {
  const ac = new AbortController();
  ac.abort();

  const { broadcast: bc } = broadcast({ signal: ac.signal });
  const consumer = bc.push();

  const batches = [];
  for await (const batch of consumer) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 0);
}

// =============================================================================
// Broadcast.from() hang fix - cancel while write blocked on backpressure
// =============================================================================

async function testBroadcastFromCancelWhileBlocked() {
  // Create a slow async source that blocks between yields
  let sourceFinished = false;
  async function* slowSource() {
    yield [new TextEncoder().encode('chunk1')];
    // Simulate a long delay - the cancel should unblock this
    await new Promise((resolve) => setTimeout(resolve, 10000));
    yield [new TextEncoder().encode('chunk2')];
    sourceFinished = true;
  }

  const { broadcast: bc } = Broadcast.from(slowSource());
  const consumer = bc.push();

  // Read the first chunk
  const iter = consumer[Symbol.asyncIterator]();
  const first = await iter.next();
  assert.strictEqual(first.done, false);

  // Cancel while the source is blocked waiting to yield the next chunk
  bc.cancel();

  // The iteration should complete (not hang)
  const next = await iter.next();
  assert.strictEqual(next.done, true);

  // Source should NOT have finished (we cancelled before chunk2)
  assert.strictEqual(sourceFinished, false);
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

Promise.all([
  testBasicBroadcast(),
  testMultipleWrites(),
  testConsumerCount(),
  testWriteSync(),
  testWritevSync(),
  testWriterEnd(),
  testWriterFail(),
  testDropOldest(),
  testDropNewest(),
  testCancelWithoutReason(),
  testCancelWithReason(),
  testBroadcastFromAsyncIterable(),
  testBroadcastFromMultipleConsumers(),
  testAbortSignal(),
  testAlreadyAbortedSignal(),
  testBroadcastFromCancelWhileBlocked(),
  testFailDetachesConsumers(),
]).then(common.mustCall());
