// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const { broadcast, ondrain, text } = require('stream/iter');

// =============================================================================
// Backpressure policies
// =============================================================================

async function testDropOldest() {
  const chunk1 = new Uint8Array(16384).fill(49); // '1'
  const chunk2 = new Uint8Array(16384).fill(50); // '2'
  const chunk3 = new Uint8Array(16384).fill(51); // '3'
  const { writer, broadcast: bc } = broadcast({
    budget: 32768,
    backpressure: 'drop-oldest',
  });
  const consumer = bc.push();

  writer.writeSync(chunk1); // 16384 < 32768
  writer.writeSync(chunk2); // 32768 >= 32768
  // Buffer full: this drops chunk1, adds chunk3
  writer.writeSync(chunk3);
  writer.endSync();

  const data = await text(consumer);
  assert.strictEqual(data, '2'.repeat(16384) + '3'.repeat(16384));
}

async function testDropNewest() {
  const kept = new Uint8Array(16384).fill(75);    // 'K'
  const dropped = new Uint8Array(16384).fill(68); // 'D'
  const { writer, broadcast: bc } = broadcast({
    budget: 16384,
    backpressure: 'drop-newest',
  });
  const consumer = bc.push();

  writer.writeSync(kept);
  // Buffer full: new write is silently discarded
  writer.writeSync(dropped);
  writer.endSync();

  const data = await text(consumer);
  assert.strictEqual(data, 'K'.repeat(16384));
}

async function testDropPoliciesReportPhysicalCapacity() {
  const chunk = new Uint8Array(16384);

  for (const backpressure of ['drop-oldest', 'drop-newest']) {
    const { writer, broadcast: bc } = broadcast({
      budget: chunk.byteLength,
      backpressure,
    });
    const iterator = bc.push()[Symbol.asyncIterator]();

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
    bc.cancel();
  }
}

// =============================================================================
// Block backpressure
// =============================================================================

async function testBlockBackpressure() {
  const kChunk = new Uint8Array(16384);
  const { writer, broadcast: bc } = broadcast({
    budget: 16384,
    backpressure: 'unbounded',
  });
  const consumer = bc.push();
  writer.writeSync(kChunk);

  // Next write should block
  let writeResolved = false;
  const writePromise = writer.write(kChunk).then(() => { writeResolved = true; });
  await new Promise(setImmediate);
  assert.strictEqual(writeResolved, false);

  // Drain consumer to unblock the pending write
  const iter = consumer[Symbol.asyncIterator]();
  const first = await iter.next();
  assert.strictEqual(first.done, false);
  await new Promise(setImmediate);
  assert.strictEqual(writeResolved, true);

  writer.endSync();
  // Drain remaining data and verify completion
  const second = await iter.next();
  assert.strictEqual(second.done, false);
  await writePromise;
}

// Verify block backpressure data flows correctly end-to-end
async function testBlockBackpressureContent() {
  const chunk1 = new Uint8Array(16384).fill(65); // 'A'
  const chunk2 = new Uint8Array(16384).fill(66); // 'B'
  const { writer, broadcast: bc } = broadcast({
    budget: 16384,
    backpressure: 'unbounded',
  });
  const consumer = bc.push();

  writer.writeSync(chunk1);
  const writePromise = writer.write(chunk2);
  await new Promise(setImmediate);

  // Read all and verify content
  const iter = consumer[Symbol.asyncIterator]();
  const first = await iter.next();
  assert.strictEqual(first.done, false);
  assert.strictEqual(first.value[0].byteLength, 16384);
  assert.strictEqual(first.value[0][0], 65); // 'A'

  await writePromise;
  writer.endSync();

  const second = await iter.next();
  assert.strictEqual(second.done, false);
  assert.strictEqual(second.value[0].byteLength, 16384);
  assert.strictEqual(second.value[0][0], 66); // 'B'

  const done = await iter.next();
  assert.strictEqual(done.done, true);
}

async function testStrictBackpressureOverflow() {
  const { writer } = broadcast({
    budget: 16384,
    backpressure: 'strict',
  });

  await writer.write(new Uint8Array(16384));
  const pending = writer.write('b');

  await assert.rejects(writer.write('c'), {
    name: 'RangeError',
    code: 'ERR_INVALID_STATE',
  });

  writer.fail();
  await assert.rejects(pending, {
    name: 'TypeError',
    code: 'ERR_INVALID_STATE',
    message: 'Invalid state: Failed',
  });
}

async function testEndDrainsPendingWrite() {
  const chunk1 = new Uint8Array(16384).fill(65); // 'A'
  const chunk2 = Uint8Array.of(66); // 'B'
  const { writer, broadcast: bc } = broadcast({
    budget: 16384,
    backpressure: 'unbounded',
  });
  const iter = bc.push()[Symbol.asyncIterator]();

  await writer.write(chunk1);
  const pendingWrite = writer.write(chunk2);
  const endPromise = writer.end();

  assert.strictEqual(writer.canWrite, null);
  assert.strictEqual(writer.writeSync('late'), false);
  await assert.rejects(writer.write('late'), {
    code: 'ERR_INVALID_STATE',
  });

  const first = await iter.next();
  assert.strictEqual(first.done, false);
  assert.strictEqual(first.value[0][0], 65);
  await pendingWrite;

  const second = await iter.next();
  assert.strictEqual(second.done, false);
  assert.strictEqual(second.value[0][0], 66);

  let endResolved = false;
  endPromise.then(common.mustCall(() => { endResolved = true; }));
  await new Promise(setImmediate);
  assert.strictEqual(endResolved, false);

  assert.strictEqual((await iter.next()).done, true);
  assert.strictEqual(await endPromise, 16385);
}

async function testEndSyncDrainsPendingWrite() {
  const chunk1 = new Uint8Array(16384).fill(65); // 'A'
  const chunk2 = Uint8Array.of(66); // 'B'
  const { writer, broadcast: bc } = broadcast({
    budget: 16384,
    backpressure: 'unbounded',
  });
  const iter = bc.push()[Symbol.asyncIterator]();

  await writer.write(chunk1);
  const pendingWrite = writer.write(chunk2);
  assert.strictEqual(writer.endSync(), -1);
  const endPromise = writer.end();

  assert.strictEqual((await iter.next()).value[0][0], 65);
  await pendingWrite;
  assert.strictEqual((await iter.next()).value[0][0], 66);
  assert.strictEqual((await iter.next()).done, true);
  assert.strictEqual(await endPromise, 16385);
  assert.strictEqual(writer.endSync(), 16385);
}

async function testAbortedPendingWriteAllowsEnd() {
  const ac = new AbortController();
  const reason = new Error('write aborted');
  const { writer, broadcast: bc } = broadcast({
    budget: 16384,
    backpressure: 'unbounded',
  });
  const iter = bc.push()[Symbol.asyncIterator]();

  await writer.write(new Uint8Array(16384));
  const pendingWrite = writer.write('blocked', { signal: ac.signal });
  const writeRejected = assert.rejects(
    pendingWrite,
    (error) => error === reason,
  );
  const endPromise = writer.end();

  ac.abort(reason);
  await writeRejected;
  assert.strictEqual((await iter.next()).done, false);
  assert.strictEqual((await iter.next()).done, true);
  assert.strictEqual(await endPromise, 16384);
}

// Writev async path
async function testWritevAsync() {
  const { writer, broadcast: bc } = broadcast({ budget: 16384 });
  const consumer = bc.push();

  await writer.writev(['hello', ' ', 'world']);
  const dataPromise = text(consumer);
  await writer.end();

  const data = await dataPromise;
  assert.strictEqual(data, 'hello world');
}

// Zero-byte writes do not consume buffer entries.
async function testZeroByteWrites() {
  const { writer, broadcast: bc } = broadcast({ budget: 16384 });
  const consumer = bc.push();

  for (let i = 0; i < 1000; i++) {
    assert.strictEqual(writer.writeSync(''), true);
    assert.strictEqual(writer.writevSync([]), true);
  }
  await writer.write('');
  await writer.writev([]);
  assert.strictEqual(writer.canWrite, true);
  writer.endSync();

  let entries = 0;
  const iterator = consumer[Symbol.asyncIterator]();
  while (!(await iterator.next()).done) entries++;
  assert.strictEqual(entries, 0);
}

// endSync falls back to end() when consumers still need to drain.
async function testEndSyncReturnValue() {
  const { writer, broadcast: bc } = broadcast({ budget: 16384 });
  const consumer = bc.push();

  writer.writeSync('hello'); // 5 bytes
  writer.writeSync(' world'); // 6 bytes
  assert.strictEqual(writer.endSync(), -1);

  const dataPromise = text(consumer);
  assert.strictEqual(await writer.end(), 11);
  assert.strictEqual(await dataPromise, 'hello world');
  assert.strictEqual(writer.endSync(), 11);
}

Promise.all([
  testDropOldest(),
  testDropNewest(),
  testDropPoliciesReportPhysicalCapacity(),
  testBlockBackpressure(),
  testBlockBackpressureContent(),
  testStrictBackpressureOverflow(),
  testEndDrainsPendingWrite(),
  testEndSyncDrainsPendingWrite(),
  testAbortedPendingWriteAllowsEnd(),
  testWritevAsync(),
  testZeroByteWrites(),
  testEndSyncReturnValue(),
]).then(common.mustCall());
