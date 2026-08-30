// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const { push, ondrain, text } = require('stream/iter');

async function testOndrain() {
  const { writer } = push({ budget: 16384 });

  // With space available, ondrain resolves immediately
  const drainResult = ondrain(writer);
  assert.ok(drainResult instanceof Promise);
  const result = await drainResult;
  assert.strictEqual(result, true);

  // After close, ondrain returns null
  writer.end();
  assert.strictEqual(ondrain(writer), null);
}

async function testDropPoliciesReportPhysicalCapacity() {
  const chunk = new Uint8Array(16384);

  for (const backpressure of ['drop-oldest', 'drop-newest']) {
    const { writer, readable } = push({
      budget: chunk.byteLength,
      backpressure,
    });
    const iterator = readable[Symbol.asyncIterator]();

    assert.strictEqual(writer.writeSync(chunk), true);
    assert.strictEqual(writer.canWrite, false);

    let drained = false;
    const drain = ondrain(writer);
    drain.then(common.mustCall(() => { drained = true; }));

    // Drop policies still accept writes despite having no physical capacity.
    assert.strictEqual(writer.writeSync(chunk), true);
    assert.strictEqual(writer.canWrite, false);
    await new Promise(setImmediate);
    assert.strictEqual(drained, false);

    assert.strictEqual((await iterator.next()).done, false);
    assert.strictEqual(await drain, true);
    assert.strictEqual(writer.canWrite, true);
    await iterator.return();
  }
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
  const kChunk = new Uint8Array(16384);
  const { writer, readable } = push({ budget: 16384 });

  // Fill the buffer so write will block
  writer.writeSync(kChunk);

  const ac = new AbortController();
  const writePromise = writer.write(kChunk, { signal: ac.signal });

  // Signal fires while write is pending
  ac.abort();

  await assert.rejects(writePromise, { name: 'AbortError' });

  // Clean up
  const end = writer.end();
  await text(readable);
  await end;
}

async function testWriteWithPreAbortedSignal() {
  const { writer, readable } = push({ budget: 16384 });

  // Pre-aborted signal should reject immediately
  await assert.rejects(
    writer.write('data', { signal: AbortSignal.abort() }),
    { name: 'AbortError' },
  );

  // Writer should still be usable for other writes
  writer.write('ok');
  const end = writer.end();
  const data = await text(readable);
  assert.strictEqual(data, 'ok');
  await end;
}

async function testCancelledWriteRemovedFromQueue() {
  const kChunk = new Uint8Array(16384);
  const { writer, readable } = push({ budget: 16384 });

  // Fill the buffer
  writer.writeSync(kChunk);

  const ac = new AbortController();
  // This write should be queued since buffer is full
  const cancelledWrite = writer.write(kChunk, { signal: ac.signal });

  // Cancel it
  ac.abort();
  await cancelledWrite.catch(() => {});

  // Drain to make room for the replacement write
  const iter = readable[Symbol.asyncIterator]();
  await iter.next();

  // The cancelled write should NOT occupy a pending slot.
  // A new write should succeed now that the buffer has room.
  await writer.write(kChunk);
  const end = writer.end();

  const result = await iter.next();
  assert.ok(!result.done);
  let totalBytes = 0;
  for (const chunk of result.value) {
    totalBytes += chunk.byteLength;
  }
  assert.strictEqual(totalBytes, 16384);
  assert.strictEqual((await iter.next()).done, true);
  await end;
}

async function testOndrainResolvesFalseOnConsumerBreak() {
  const kChunk = new Uint8Array(16384);
  const { writer, readable } = push({ budget: 16384 });

  // Fill the buffer so canWrite = false
  writer.writeSync(kChunk);

  // Also queue a pending write so that reading one chunk
  // doesn't clear backpressure (the pending write refills the buffer)
  const pendingWrite = writer.write(kChunk);

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
  const kChunk = new Uint8Array(16384);
  const { writer, readable } = push({ budget: 16384 });

  // Fill the buffer so canWrite = false
  writer.writeSync(kChunk);

  // Also queue a pending write so that reading one chunk
  // doesn't clear backpressure (the pending write refills the buffer)
  const pendingWrite = writer.write(kChunk);

  // Start a drain wait - still at capacity
  const drainPromise = ondrain(writer);

  // Consumer throws via iterator.throw() before draining enough
  // to clear backpressure. The drain should reject.
  const iter = readable[Symbol.asyncIterator]();
  const err = new Error('consumer error');
  const drainRejects = assert.rejects(drainPromise, (e) => e === err);
  const pendingWriteRejects = pendingWrite.catch(() => {});
  await assert.rejects(
    () => iter.throw(err),
    (e) => e === err,
  );

  await drainRejects;
  await pendingWriteRejects; // Ignore write rejection
}

async function testWritev() {
  const { writer, readable } = push({ budget: 16384 });
  const enc = new TextEncoder();
  writer.writev([enc.encode('hel'), enc.encode('lo')]);
  writer.endSync();
  const result = await text(readable);
  assert.strictEqual(result, 'hello');
}

async function testWritevSync() {
  const { writer, readable } = push({ budget: 16384 });
  const enc = new TextEncoder();
  assert.strictEqual(writer.writevSync([enc.encode('hel'), enc.encode('lo')]), true);
  writer.endSync();
  const result = await text(readable);
  assert.strictEqual(result, 'hello');
}

async function testWritevSyncInvalidChunkDoesNotQueue() {
  const { writer, readable } = push({ budget: 16384 });

  assert.throws(
    () => writer.writevSync([1]),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );

  const iter = readable[Symbol.asyncIterator]();
  const next = iter.next();
  const result = await Promise.race([
    next.then(() => 'resolved'),
    new Promise((resolve) => setImmediate(resolve, 'pending')),
  ]);
  assert.strictEqual(result, 'pending');

  writer.endSync();
  const end = await next;
  assert.strictEqual(end.value, undefined);
  assert.strictEqual(end.done, true);
}

async function testWritevMixedTypes() {
  const { writer, readable } = push({ budget: 16384 });
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

async function testEndWithPreAbortedSignal() {
  const { writer, readable } = push();
  const reason = new Error('end aborted');

  writer.writeSync('hello');
  await assert.rejects(
    writer.end({ signal: AbortSignal.abort(reason) }),
    (error) => error === reason,
  );

  // A rejected end must leave the writer open.
  writer.writeSync(' world');
  const consume = text(readable);
  assert.strictEqual(await writer.end(), 11);
  assert.strictEqual(await consume, 'hello world');
}

async function testEndSignalAbortWhileDraining() {
  const { writer, readable } = push();
  const controller = new AbortController();
  const reason = new Error('end aborted while draining');

  writer.writeSync('hello');
  const abortedEnd = writer.end({ signal: controller.signal });
  controller.abort(reason);

  await assert.rejects(abortedEnd, (error) => error === reason);

  // Aborting the operation does not undo the end-of-stream signal.
  const completedEnd = writer.end();
  assert.strictEqual(await text(readable), 'hello');
  assert.strictEqual(await completedEnd, 5);
}

async function testFactorySignalAbortWhileDraining() {
  const controller = new AbortController();
  const reason = new Error('stream aborted while draining');
  const { writer, readable } = push({ signal: controller.signal });

  writer.writeSync('hello');
  const end = writer.end();
  const endRejected = assert.rejects(end, (error) => error === reason);
  controller.abort(reason);

  await endRejected;
  await assert.rejects(text(readable), (error) => error === reason);
  await assert.rejects(writer.end(), (error) => error === reason);
}

async function testEndAfterEndSyncWaitsForDrain() {
  const { writer, readable } = push();
  writer.writeSync('hello');
  assert.strictEqual(writer.endSync(), -1);

  let ended = false;
  const end = writer.end().then((n) => {
    ended = true;
    return n;
  });

  await Promise.resolve();
  assert.strictEqual(ended, false);

  // eslint-disable-next-line no-unused-vars
  for await (const _ of readable) { /* drain */ }
  assert.strictEqual(await end, 5);
}

async function testWriteUint8Array() {
  const { writer, readable } = push();
  writer.write(new Uint8Array([72, 73])); // 'HI'
  writer.endSync();
  const result = await text(readable);
  assert.strictEqual(result, 'HI');
}

async function testOndrainWaitsForDrain() {
  const kChunk = new Uint8Array(16384);
  const { writer, readable } = push({ budget: 16384 });
  writer.writeSync(kChunk); // Fills budget

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
  const kChunk = new Uint8Array(16384);
  const { writer, readable } = push({ budget: 16384 });
  writer.writeSync(kChunk);

  const iter = readable[Symbol.asyncIterator]();
  const err = new Error('consumer boom');
  await assert.rejects(
    () => iter.throw(err),
    (e) => e === err,
  );

  // Subsequent async writes should reject with the consumer's error
  await assert.rejects(
    () => writer.write('x'),
    { message: 'consumer boom' },
  );
}

async function testConsumerThrowRejectsWithThrownError() {
  const { readable } = push();

  const iter = readable[Symbol.asyncIterator]();
  const err = new Error('boom');

  await assert.rejects(
    () => iter.throw(err),
    (e) => e === err,
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

// iterator.return() resolves a pending read with done:true
async function testConsumerReturnResolvesPendingRead() {
  const { readable } = push();

  const iter = readable[Symbol.asyncIterator]();
  const readPromise = iter.next();

  await new Promise(setImmediate);

  const returnResult = await iter.return();
  assert.strictEqual(returnResult.value, undefined);
  assert.strictEqual(returnResult.done, true);

  const readResult = await readPromise;
  assert.strictEqual(readResult.value, undefined);
  assert.strictEqual(readResult.done, true);
}

async function testEndRejectsAfterConsumerReturn() {
  const { writer, readable } = push();
  writer.writeSync('data');
  const iter = readable[Symbol.asyncIterator]();

  await iter.return();

  await assert.rejects(
    writer.end({ signal: AbortSignal.timeout(common.platformTimeout(100)) }),
    { code: 'ERR_INVALID_STATE' },
  );
  assert.strictEqual((await iter.next()).done, true);
}

// iterator.throw() rejects a pending read with the thrown error
async function testConsumerThrowRejectsPendingRead() {
  const { readable } = push();

  const iter = readable[Symbol.asyncIterator]();
  const readPromise = iter.next();

  await new Promise(setImmediate);

  const err = new Error('consumer read boom');
  const readRejects = assert.rejects(
    () => readPromise,
    (e) => e === err,
  );
  await assert.rejects(
    () => iter.throw(err),
    (e) => e === err,
  );

  await readRejects;
}

// end() drains writes that were already pending, then waits for EOF to be read.
async function testEndDrainsPendingWrites() {
  const kChunk = new Uint8Array(16384);
  const { writer, readable } = push({ budget: 16384, backpressure: 'unbounded' });
  writer.writeSync(kChunk); // fill budget

  // This write blocks on backpressure
  const writePromise = writer.write(kChunk);
  const endPromise = writer.end();
  await assert.rejects(writer.write(kChunk), { code: 'ERR_INVALID_STATE' });

  let ended = false;
  endPromise.then(common.mustCall(() => { ended = true; }));
  const iterator = readable[Symbol.asyncIterator]();

  assert.strictEqual((await iterator.next()).done, false);
  await writePromise;
  assert.strictEqual((await iterator.next()).done, false);
  await Promise.resolve();
  assert.strictEqual(ended, false);

  assert.strictEqual((await iterator.next()).done, true);
  assert.strictEqual(await endPromise, kChunk.byteLength * 2);
  assert.strictEqual(ended, true);
}

async function testEndWaitsForEofPull() {
  const { writer, readable } = push();
  writer.writeSync('hello');
  const endPromise = writer.end();
  let ended = false;
  endPromise.then(common.mustCall(() => { ended = true; }));
  const iterator = readable[Symbol.asyncIterator]();

  const data = await iterator.next();
  assert.strictEqual(data.done, false);
  await Promise.resolve();
  assert.strictEqual(ended, false);

  assert.strictEqual((await iterator.next()).done, true);
  await endPromise;
  assert.strictEqual(ended, true);
}

async function testEndIdempotentWhenClosed() {
  const { writer, readable } = push({ budget: 16384 });
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
  const { writer, readable } = push({ budget: 16384 });
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

async function testAsyncDisposeWaitsAfterEndSync() {
  const { writer, readable } = push({ budget: 16384 });
  writer.writeSync('hello');
  assert.strictEqual(writer.endSync(), -1);

  let disposed = false;
  const disposal = writer[Symbol.asyncDispose]().then(() => {
    disposed = true;
  });
  await Promise.resolve();
  assert.strictEqual(disposed, false);

  assert.strictEqual(await text(readable), 'hello');
  await disposal;
  assert.strictEqual(disposed, true);
}

async function testSyncDispose() {
  const { writer, readable } = push({ budget: 16384 });
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
  const { writer, readable } = push({ budget: 16384 });
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

async function testFailRejectsFutureReadWithFalsyReason() {
  for (const reason of [0, null]) {
    const { writer, readable } = push();

    writer.fail(reason);

    const iter = readable[Symbol.asyncIterator]();
    await iter.next().then(
      common.mustNotCall(),
      common.mustCall((rejection) => {
        assert.strictEqual(rejection, reason);
      }),
    );
  }
}

async function testFailRejectsPendingReadWithFalsyReason() {
  const { writer, readable } = push();

  const iter = readable[Symbol.asyncIterator]();
  const readPromise = iter.next();

  await new Promise(setImmediate);

  writer.fail(false);
  await readPromise.then(
    common.mustNotCall(),
    common.mustCall((reason) => {
      assert.strictEqual(reason, false);
    }),
  );
}

Promise.all([
  testOndrain(),
  testDropPoliciesReportPhysicalCapacity(),
  testOndrainNonDrainable(),
  testWriteWithSignalRejects(),
  testWriteWithPreAbortedSignal(),
  testCancelledWriteRemovedFromQueue(),
  testOndrainResolvesFalseOnConsumerBreak(),
  testOndrainRejectsOnConsumerThrow(),
  testWritev(),
  testWritevSync(),
  testWritevSyncInvalidChunkDoesNotQueue(),
  testWritevMixedTypes(),
  testWriteAfterEnd(),
  testWriteAfterFail(),
  testOndrainProtocolErrorPropagates(),
  testFail(),
  testEndAsyncReturnValue(),
  testEndWithPreAbortedSignal(),
  testEndSignalAbortWhileDraining(),
  testFactorySignalAbortWhileDraining(),
  testEndAfterEndSyncWaitsForDrain(),
  testWriteUint8Array(),
  testOndrainWaitsForDrain(),
  testConsumerThrowRejectsWrites(),
  testConsumerThrowRejectsWithThrownError(),
  testEndResolvesPendingRead(),
  testFailRejectsPendingRead(),
  testFailRejectsFutureReadWithFalsyReason(),
  testFailRejectsPendingReadWithFalsyReason(),
  testConsumerReturnResolvesPendingRead(),
  testEndRejectsAfterConsumerReturn(),
  testConsumerThrowRejectsPendingRead(),
  testEndDrainsPendingWrites(),
  testEndWaitsForEofPull(),
  testEndIdempotentWhenClosed(),
  testEndRejectsWhenErrored(),
  testAsyncDispose(),
  testAsyncDisposeWaitsAfterEndSync(),
  testSyncDispose(),
]).then(common.mustCall());
