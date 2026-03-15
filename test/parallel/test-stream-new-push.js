'use strict';

const common = require('../common');
const assert = require('assert');
const { push, text, ondrain } = require('stream/new');

async function testBasicWriteRead() {
  const { writer, readable } = push();

  writer.write('hello');
  writer.end();

  const data = await text(readable);
  assert.strictEqual(data, 'hello');
}

async function testMultipleWrites() {
  const { writer, readable } = push({ highWaterMark: 10 });

  writer.write('a');
  writer.write('b');
  writer.write('c');
  writer.end();

  const data = await text(readable);
  assert.strictEqual(data, 'abc');
}

async function testDesiredSize() {
  const { writer } = push({ highWaterMark: 3 });

  assert.strictEqual(writer.desiredSize, 3);
  writer.writeSync('a');
  assert.strictEqual(writer.desiredSize, 2);
  writer.writeSync('b');
  assert.strictEqual(writer.desiredSize, 1);
  writer.writeSync('c');
  assert.strictEqual(writer.desiredSize, 0);

  writer.end();
  assert.strictEqual(writer.desiredSize, null);
}

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

  writer.writeSync('first');
  writer.writeSync('second');
  // This should drop 'first'
  writer.writeSync('third');
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

  writer.writeSync('kept');
  // This is silently dropped
  writer.writeSync('dropped');
  writer.end();

  const data = await text(readable);
  assert.strictEqual(data, 'kept');
}

async function testWriterEnd() {
  const { writer, readable } = push();

  const totalBytes = writer.endSync();
  assert.strictEqual(totalBytes, 0);

  const batches = [];
  for await (const batch of readable) {
    batches.push(batch);
  }
  assert.strictEqual(batches.length, 0);
}

async function testWriterFail() {
  const { writer, readable } = push();

  writer.fail(new Error('test fail'));

  await assert.rejects(
    async () => {
      // eslint-disable-next-line no-unused-vars
      for await (const _ of readable) {
        assert.fail('Should not reach here');
      }
    },
    { message: 'test fail' },
  );
}

async function testConsumerBreak() {
  const { writer, readable } = push({ highWaterMark: 10 });

  writer.writeSync('a');
  writer.writeSync('b');
  writer.writeSync('c');

  // Break after first batch
  // eslint-disable-next-line no-unused-vars
  for await (const _ of readable) {
    break;
  }

  // Writer should now see null desiredSize
  assert.strictEqual(writer.desiredSize, null);
}

async function testAbortSignal() {
  const ac = new AbortController();
  const { readable } = push({ signal: ac.signal });

  ac.abort();

  await assert.rejects(
    async () => {
      // eslint-disable-next-line no-unused-vars
      for await (const _ of readable) {
        assert.fail('Should not reach here');
      }
    },
    (err) => err.name === 'AbortError',
  );
}

async function testOndrain() {
  const { writer } = push({ highWaterMark: 1 });

  // With space available, ondrain resolves immediately
  const drainResult = ondrain(writer);
  assert.ok(drainResult instanceof Promise);
  const result = await drainResult;
  assert.strictEqual(result, true);

  // After close, ondrain returns null
  writer.end();
  assert.strictEqual(ondrain(writer), null);
}

async function testOndainNonDrainable() {
  // Non-drainable objects return null
  assert.strictEqual(ondrain(null), null);
  assert.strictEqual(ondrain({}), null);
  assert.strictEqual(ondrain('string'), null);
}

async function testPushWithTransforms() {
  const upper = (chunks) => {
    if (chunks === null) return null;
    return chunks.map((c) => {
      const str = new TextDecoder().decode(c);
      return new TextEncoder().encode(str.toUpperCase());
    });
  };

  const { writer, readable } = push(upper);

  writer.write('hello');
  writer.end();

  const data = await text(readable);
  assert.strictEqual(data, 'HELLO');
}

// =============================================================================
// Per-operation signal tests
// =============================================================================

async function testWriteWithSignalRejects() {
  const { writer, readable } = push({ highWaterMark: 1 });

  // Fill the buffer so write will block
  writer.writeSync('a');

  const ac = new AbortController();
  const writePromise = writer.write('b', { signal: ac.signal });

  // Signal fires while write is pending
  ac.abort();

  await assert.rejects(writePromise, (err) => err.name === 'AbortError');

  // Clean up
  writer.end();
  // eslint-disable-next-line no-unused-vars
  for await (const _ of readable) { break; }
}

async function testWriteWithPreAbortedSignal() {
  const { writer, readable } = push({ highWaterMark: 1 });

  const ac = new AbortController();
  ac.abort();

  // Pre-aborted signal should reject immediately
  await assert.rejects(
    writer.write('data', { signal: ac.signal }),
    (err) => err.name === 'AbortError',
  );

  // Writer should still be usable for other writes
  writer.write('ok');
  writer.end();
  const data = await text(readable);
  assert.strictEqual(data, 'ok');
}

async function testCancelledWriteRemovedFromQueue() {
  const { writer, readable } = push({ highWaterMark: 1 });

  // Fill the buffer
  writer.writeSync('first');

  const ac = new AbortController();
  // This write should be queued since buffer is full
  const cancelledWrite = writer.write('cancelled', { signal: ac.signal });

  // Cancel it
  ac.abort();
  await cancelledWrite.catch(() => {});

  // Drain 'first' to make room for the replacement write
  const iter = readable[Symbol.asyncIterator]();
  await iter.next();

  // The cancelled write should NOT occupy a pending slot.
  // A new write should succeed now that the buffer has room.
  await writer.write('second');
  writer.end();

  const result = await iter.next();
  // 'second' should be the next (and only remaining) chunk
  const decoder = new TextDecoder();
  let data = '';
  for (const chunk of result.value) {
    data += decoder.decode(chunk, { stream: true });
  }
  assert.strictEqual(data, 'second');
  await iter.return();
}

// =============================================================================
// Ondrain cleanup on consumer termination
// =============================================================================

async function testOndrainResolvesFalseOnConsumerBreak() {
  const { writer, readable } = push({ highWaterMark: 1 });

  // Fill the buffer so desiredSize = 0
  writer.writeSync('a');

  // Also queue a pending write so that reading one chunk
  // doesn't clear backpressure (the pending write refills the slot)
  const pendingWrite = writer.write('b');

  // Start a drain wait - still at capacity
  const drainPromise = ondrain(writer);

  // Consumer returns without draining enough to clear backpressure
  const iter = readable[Symbol.asyncIterator]();
  await iter.return();

  // Ondrain should resolve false since the consumer terminated
  const result = await drainPromise;
  assert.strictEqual(result, false);
  await pendingWrite.catch(() => {}); // Ignore write rejection
}

async function testOndrainRejectsOnConsumerThrow() {
  const { writer, readable } = push({ highWaterMark: 1 });

  // Fill the buffer so desiredSize = 0
  writer.writeSync('a');

  // Also queue a pending write so that reading one chunk
  // doesn't clear backpressure (the pending write refills the slot)
  const pendingWrite = writer.write('b');

  // Start a drain wait - still at capacity
  const drainPromise = ondrain(writer);

  // Consumer throws via iterator.throw() before draining enough
  // to clear backpressure. The drain should reject.
  const iter = readable[Symbol.asyncIterator]();
  await iter.throw(new Error('consumer error'));

  await assert.rejects(drainPromise, /consumer error/);
  await pendingWrite.catch(() => {}); // Ignore write rejection
}

Promise.all([
  testBasicWriteRead(),
  testMultipleWrites(),
  testDesiredSize(),
  testStrictBackpressure(),
  testDropOldest(),
  testDropNewest(),
  testWriterEnd(),
  testWriterFail(),
  testConsumerBreak(),
  testAbortSignal(),
  testOndrain(),
  testOndainNonDrainable(),
  testPushWithTransforms(),
  testWriteWithSignalRejects(),
  testWriteWithPreAbortedSignal(),
  testCancelledWriteRemovedFromQueue(),
  testOndrainResolvesFalseOnConsumerBreak(),
  testOndrainRejectsOnConsumerThrow(),
]).then(common.mustCall());
