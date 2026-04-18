// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const { push, ondrain, text } = require('stream/iter');

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

async function testOndrainNonDrainable() {
  // Non-drainable objects return null
  assert.strictEqual(ondrain(null), null);
  assert.strictEqual(ondrain({}), null);
  assert.strictEqual(ondrain('string'), null);
}

async function testOndrainProtocolErrorPropagates() {
  const badDrainable = {
    [Symbol.for('Stream.drainableProtocol')]() {
      throw new Error('protocol error');
    },
  };
  assert.throws(
    () => ondrain(badDrainable),
    { message: 'protocol error' },
  );
}

async function testWriteWithSignalRejects() {
  const { writer, readable } = push({ highWaterMark: 1 });

  // Fill the buffer so write will block
  writer.writeSync('a');

  const ac = new AbortController();
  const writePromise = writer.write('b', { signal: ac.signal });

  // Signal fires while write is pending
  ac.abort();

  await assert.rejects(writePromise, { name: 'AbortError' });

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
    { name: 'AbortError' },
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

async function testWritev() {
  const { writer, readable } = push({ highWaterMark: 10 });
  const enc = new TextEncoder();
  writer.writev([enc.encode('hel'), enc.encode('lo')]);
  writer.endSync();
  const result = await text(readable);
  assert.strictEqual(result, 'hello');
}

async function testWritevSync() {
  const { writer, readable } = push({ highWaterMark: 10 });
  const enc = new TextEncoder();
  assert.strictEqual(writer.writevSync([enc.encode('hel'), enc.encode('lo')]), true);
  writer.endSync();
  const result = await text(readable);
  assert.strictEqual(result, 'hello');
}

async function testWritevMixedTypes() {
  const { writer, readable } = push({ highWaterMark: 10 });
  // Mix strings and Uint8Arrays
  writer.writev(['hel', new TextEncoder().encode('lo')]);
  writer.endSync();
  const result = await text(readable);
  assert.strictEqual(result, 'hello');
}

async function testWriteAfterEnd() {
  const { writer } = push();
  writer.endSync();
  // Sync write after end returns false
  assert.strictEqual(writer.writeSync('fail'), false);
  // Async write after end rejects
  await assert.rejects(
    () => writer.write('fail'),
    { code: 'ERR_INVALID_STATE' },
  );
}

async function testWriteAfterFail() {
  const { writer } = push();
  writer.fail(new Error('failed'));
  // Sync write after fail returns false
  assert.strictEqual(writer.writeSync('fail'), false);
  // Async write after fail rejects with the stored error
  await assert.rejects(
    () => writer.write('fail'),
    { message: 'failed' },
  );
}

async function testFail() {
  const { writer, readable } = push();
  writer.writeSync('hello');
  writer.fail(new Error('boom'));
  // Second fail is a no-op (already errored)
  writer.fail(new Error('boom2'));
  await assert.rejects(async () => {
    // eslint-disable-next-line no-unused-vars
    for await (const _ of readable) { /* consume */ }
  }, { message: 'boom' });
}

async function testEndAsyncReturnValue() {
  const { writer, readable } = push();
  writer.writeSync('hello');
  // Start consuming concurrently (end() waits for drain)
  const consume = (async () => {
    // eslint-disable-next-line no-unused-vars
    for await (const _ of readable) { /* drain */ }
  })();
  const total = await writer.end();
  assert.strictEqual(total, 5);
  await consume;
}

async function testWriteUint8Array() {
  const { writer, readable } = push();
  writer.write(new Uint8Array([72, 73])); // 'HI'
  writer.endSync();
  const result = await text(readable);
  assert.strictEqual(result, 'HI');
}

async function testOndrainWaitsForDrain() {
  const { writer, readable } = push({ highWaterMark: 1 });
  writer.writeSync('a'); // Fills buffer

  let drainState = 'pending';
  const drainPromise = ondrain(writer).then((v) => { drainState = v; });

  await new Promise(setImmediate);
  assert.strictEqual(drainState, 'pending'); // Still waiting

  // Read to drain
  const iter = readable[Symbol.asyncIterator]();
  await iter.next();

  await drainPromise;
  assert.strictEqual(drainState, true);
  writer.endSync();
}

// Consumer throw causes subsequent writes to reject with consumer's error
async function testConsumerThrowRejectsWrites() {
  const { writer, readable } = push({ highWaterMark: 1 });
  writer.writeSync('a');

  const iter = readable[Symbol.asyncIterator]();
  await iter.throw(new Error('consumer boom'));

  // Subsequent async writes should reject with the consumer's error
  await assert.rejects(
    () => writer.write('x'),
    { message: 'consumer boom' },
  );
}

// end() resolves a pending read as done:true
async function testEndResolvesPendingRead() {
  const { writer, readable } = push();

  // Consumer starts reading — blocks because buffer is empty
  const iter = readable[Symbol.asyncIterator]();
  const readPromise = iter.next();

  // Give the read a tick to enter the pending state
  await new Promise(setImmediate);

  // End the writer — should resolve the pending read with done:true
  writer.endSync();
  const result = await readPromise;
  assert.strictEqual(result.done, true);
}

// fail() rejects a pending read with the error
async function testFailRejectsPendingRead() {
  const { writer, readable } = push();

  const iter = readable[Symbol.asyncIterator]();
  const readPromise = iter.next();

  await new Promise(setImmediate);

  writer.fail(new Error('fail during read'));
  await assert.rejects(
    () => readPromise,
    { message: 'fail during read' },
  );
}

// end() while writes are pending rejects those writes
async function testEndRejectsPendingWrites() {
  const { writer, readable } = push({ highWaterMark: 1, backpressure: 'block' });
  writer.writeSync('a'); // fill buffer

  // This write blocks on backpressure
  const writePromise = writer.write('b');

  await new Promise(setImmediate);

  // Ending should reject the pending write
  writer.endSync();

  await assert.rejects(
    () => writePromise,
    { code: 'ERR_INVALID_STATE' },
  );

  // Clean up: drain the readable
  // eslint-disable-next-line no-unused-vars
  for await (const _ of readable) { break; }
}

async function testEndIdempotentWhenClosed() {
  const { writer, readable } = push({ highWaterMark: 10 });
  await writer.write('hello');
  // Start consuming concurrently (end() waits for drain)
  const consume = (async () => {
    // eslint-disable-next-line no-unused-vars
    for await (const _ of readable) { /* drain */ }
  })();
  const first = await writer.end();
  assert.strictEqual(first, 5);
  // Second end() should resolve with same byte count (idempotent)
  const second = await writer.end();
  assert.strictEqual(second, 5);
  await consume;
}

async function testAsyncDispose() {
  const { writer, readable } = push({ highWaterMark: 10 });
  writer.writeSync('hello');
  // Symbol.asyncDispose calls fail() with no argument
  await writer[Symbol.asyncDispose]();
  // Writer is now errored, writes should fail
  assert.strictEqual(writer.writeSync('fail'), false);
  // Drain readable
  try {
    // eslint-disable-next-line no-unused-vars
    for await (const _ of readable) { /* consume */ }
  } catch {
    // Expected - reader sees the error
  }
}

async function testSyncDispose() {
  const { writer, readable } = push({ highWaterMark: 10 });
  writer.writeSync('hello');
  // Symbol.dispose calls fail() with no argument
  writer[Symbol.dispose]();
  // Writer is now errored, writes should fail
  assert.strictEqual(writer.writeSync('fail'), false);
  // Drain readable
  try {
    // eslint-disable-next-line no-unused-vars
    for await (const _ of readable) { /* consume */ }
  } catch {
    // Expected
  }
}

async function testEndRejectsWhenErrored() {
  const { writer, readable } = push({ highWaterMark: 10 });
  await writer.write('hello');
  const err = new Error('boom');
  await writer.fail(err);
  // end() after fail should reject with the stored error
  await assert.rejects(
    () => writer.end(),
    (e) => e === err,
  );
  // Drain readable
  try {
    // eslint-disable-next-line no-unused-vars
    for await (const _ of readable) { break; }
  } catch {
    // Expected - reader may see the error
  }
}

Promise.all([
  testOndrain(),
  testOndrainNonDrainable(),
  testWriteWithSignalRejects(),
  testWriteWithPreAbortedSignal(),
  testCancelledWriteRemovedFromQueue(),
  testOndrainResolvesFalseOnConsumerBreak(),
  testOndrainRejectsOnConsumerThrow(),
  testWritev(),
  testWritevSync(),
  testWritevMixedTypes(),
  testWriteAfterEnd(),
  testWriteAfterFail(),
  testOndrainProtocolErrorPropagates(),
  testFail(),
  testEndAsyncReturnValue(),
  testWriteUint8Array(),
  testOndrainWaitsForDrain(),
  testConsumerThrowRejectsWrites(),
  testEndResolvesPendingRead(),
  testFailRejectsPendingRead(),
  testEndRejectsPendingWrites(),
  testEndIdempotentWhenClosed(),
  testEndRejectsWhenErrored(),
  testAsyncDispose(),
  testSyncDispose(),
]).then(common.mustCall());
