// Flags: --experimental-stream-iter
'use strict';

// Tests for classic Writable stream interop with the stream/iter API
// via fromWritable().

const common = require('../common');
const assert = require('assert');
const { EventEmitter } = require('events');
const { Writable } = require('stream');
const { setImmediate } = require('timers/promises');
const {
  from,
  fromWritable,
  pipeTo,
  text,
  ondrain,
} = require('stream/iter');

// =============================================================================
// fromWritable() is exported from stream/iter
// =============================================================================

function testFunctionExists() {
  assert.strictEqual(typeof fromWritable, 'function');
}

// =============================================================================
// Default policy is strict
// =============================================================================

async function testDefaultIsStrict() {
  const writable = new Writable({
    highWaterMark: 1024,
    write(chunk, encoding, cb) { cb(); },
  });

  const writer = fromWritable(writable);
  // Should work fine when buffer has room
  await writer.write('hello');
  await writer.end();
}

// =============================================================================
// Basic write: pipeTo through the adapter (block policy for pipeTo compat)
// =============================================================================

async function testBasicWrite() {
  const chunks = [];
  const writable = new Writable({
    write(chunk, encoding, cb) {
      chunks.push(Buffer.from(chunk));
      cb();
    },
  });

  const writer = fromWritable(writable, { backpressure: 'unbounded' });
  await pipeTo(from('hello world'), writer);

  assert.strictEqual(Buffer.concat(chunks).toString(), 'hello world');
}

// =============================================================================
// write() resolves when no backpressure (strict)
// =============================================================================

async function testWriteNoDrain() {
  const chunks = [];
  const writable = new Writable({
    highWaterMark: 1024,
    write(chunk, encoding, cb) {
      chunks.push(Buffer.from(chunk));
      cb();
    },
  });

  const writer = fromWritable(writable);
  await writer.write('hello');
  await writer.write(' world');
  await writer.end();

  assert.strictEqual(Buffer.concat(chunks).toString(), 'hello world');
}

// =============================================================================
// block: write() waits for drain when backpressure is active
// =============================================================================

async function testBlockWaitsForDrain() {
  const chunks = [];
  const writable = new Writable({
    highWaterMark: 1,  // Very small buffer
    write(chunk, encoding, cb) {
      chunks.push(Buffer.from(chunk));
      // Delay callback to simulate slow consumer
      setTimeout(cb, 10);
    },
  });

  const writer = fromWritable(writable, { backpressure: 'unbounded' });

  await writer.write('a');
  await writer.write('b');
  await writer.write('c');
  await writer.end();

  assert.strictEqual(Buffer.concat(chunks).toString(), 'abc');
}

// =============================================================================
// block: stream error rejects pending write
// =============================================================================

async function testBlockErrorRejectsPendingWrite() {
  const writable = new Writable({
    highWaterMark: 1,
    write(chunk, enc, cb) {
      // Never call cb -- simulate stuck write
    },
  });

  const writer = fromWritable(writable, { backpressure: 'unbounded' });

  // The first write fills the buffer; the second waits for drain.
  await writer.write('a');
  const writePromise = writer.write('data that will block');

  // Destroy with error while write is pending
  writable.destroy(new Error('stream broke'));

  await assert.rejects(writePromise, { message: 'stream broke' });
}

async function testErrorBeforeBackpressureIsStored() {
  const writable = new Writable({
    write(chunk, enc, cb) { cb(); },
  });
  const writer = fromWritable(writable);
  const reason = new Error('early stream error');
  const closed = new Promise((resolve) => writable.once('close', resolve));

  writable.destroy(reason);
  await assert.rejects(writer.write('late'), (error) => error === reason);
  await closed;

  await assert.rejects(writer.end(), (error) => error === reason);
}

async function testAlreadyErroredWritablePreservesReason() {
  const writable = new Writable({
    write(chunk, enc, cb) { cb(); },
  });
  const reason = new Error('existing stream error');
  const closed = new Promise((resolve) => writable.once('close', resolve));
  writable.destroy(reason);

  const writer = fromWritable(writable);
  await assert.rejects(writer.write('late'), (error) => error === reason);
  await closed;
}

async function testCleanDestroyRejectsQueuedOperations() {
  const writable = new Writable({
    highWaterMark: 1,
    write(chunk, enc, cb) {},
  });
  const writer = fromWritable(writable);

  await writer.write('a');
  const pending = writer.write('b');
  const ending = writer.end();
  writable.destroy();

  await assert.rejects(pending, { name: 'AbortError' });
  await assert.rejects(ending, { name: 'AbortError' });
}

async function testPreAbortedWriteSignalsDoNotCommit() {
  let writes = 0;
  const writable = new Writable({
    write(chunk, enc, cb) {
      writes++;
      cb();
    },
  });
  const writer = fromWritable(writable);
  const reason = { canceled: true };
  const signal = AbortSignal.abort(reason);

  await assert.rejects(
    writer.write('a', { signal }),
    (error) => error === reason,
  );
  await assert.rejects(
    writer.writev([new Uint8Array([98])], { signal }),
    (error) => error === reason,
  );
  assert.strictEqual(writes, 0);
  await writer.end();
}

async function testPendingWriteSignalRemovesOperation() {
  const callbacks = [];
  const chunks = [];
  const writable = new Writable({
    highWaterMark: 1,
    write(chunk, enc, cb) {
      chunks.push(Buffer.from(chunk));
      callbacks.push(cb);
    },
  });
  const writer = fromWritable(writable);

  await writer.write('a');
  const controller = new AbortController();
  const canceled = writer.write('b', { signal: controller.signal });
  controller.abort('stop');
  await assert.rejects(canceled, (reason) => reason === 'stop');
  assert.deepStrictEqual(chunks.map((chunk) => chunk.toString()), ['a']);

  // Cancellation frees the strict policy's single pending-operation slot.
  const replacement = writer.write('c');
  callbacks.shift()();
  await replacement;
  assert.deepStrictEqual(chunks.map((chunk) => chunk.toString()), ['a', 'c']);
  const ending = writer.end();
  callbacks.shift()();
  await ending;
}

async function testZeroHighWaterMarkAcceptsFirstWrite() {
  const callbacks = [];
  const chunks = [];
  const writable = new Writable({
    highWaterMark: 0,
    write(chunk, enc, cb) {
      chunks.push(Buffer.from(chunk));
      callbacks.push(cb);
    },
  });
  const writer = fromWritable(writable);

  await writer.write('a');
  assert.deepStrictEqual(chunks.map((chunk) => chunk.toString()), ['a']);
  const pending = writer.write('b');
  callbacks.shift()();
  await pending;
  assert.deepStrictEqual(chunks.map((chunk) => chunk.toString()), ['a', 'b']);
  const ending = writer.end();
  callbacks.shift()();
  await ending;
}

async function testDuckWritableLatchesWriteBackpressure() {
  const writable = new EventEmitter();
  const chunks = [];
  writable.write = (chunk) => {
    chunks.push(Buffer.from(chunk));
    return false;
  };
  writable.end = () => {
    writable.writableFinished = true;
    writable.emit('finish');
  };
  writable.destroy = () => {
    writable.destroyed = true;
    writable.emit('close');
  };

  const writer = fromWritable(writable);
  await writer.write('a');
  const pending = writer.write('b');
  await setImmediate();
  assert.deepStrictEqual(chunks.map((chunk) => chunk.toString()), ['a']);
  writable.emit('drain');
  await pending;
  assert.deepStrictEqual(chunks.map((chunk) => chunk.toString()), ['a', 'b']);
  await writer.end();
}

async function testPendingWritesRemainFifoDuringDrain() {
  const callbacks = [];
  const chunks = [];
  const writable = new Writable({
    highWaterMark: 1,
    write(chunk, enc, cb) {
      chunks.push(Buffer.from(chunk));
      callbacks.push(cb);
    },
  });
  const writer = fromWritable(writable, { backpressure: 'unbounded' });

  await writer.write('a');
  let reentrant;
  writable.once('drain', () => {
    reentrant = writer.write('c');
  });
  const pending = writer.write('b');
  callbacks.shift()();
  await pending;
  assert.deepStrictEqual(chunks.map((chunk) => chunk.toString()), ['a', 'b']);
  callbacks.shift()();
  await reentrant;
  assert.deepStrictEqual(chunks.map((chunk) => chunk.toString()),
                         ['a', 'b', 'c']);
  const ending = writer.end();
  callbacks.shift()();
  await ending;
}

async function testOndrainWaitsForEntirePendingQueue() {
  const callbacks = [];
  const writable = new Writable({
    highWaterMark: 1,
    write(chunk, enc, cb) { callbacks.push(cb); },
  });
  const writer = fromWritable(writable, { backpressure: 'unbounded' });

  await writer.write('a');
  const second = writer.write('b');
  const third = writer.write('c');
  let drained = false;
  const draining = ondrain(writer).then((value) => {
    drained = value;
  });

  callbacks.shift()();
  await second;
  await setImmediate();
  assert.strictEqual(drained, false);
  callbacks.shift()();
  await third;
  await setImmediate();
  assert.strictEqual(drained, false);
  callbacks.shift()();
  await draining;
  assert.strictEqual(drained, true);
  await writer.end();
}

async function testEndSignal() {
  const callbacks = [];
  const writable = new Writable({
    highWaterMark: 1,
    write(chunk, enc, cb) { callbacks.push(cb); },
  });
  const writer = fromWritable(writable);
  const preAborted = AbortSignal.abort('before end');

  await assert.rejects(
    writer.end({ signal: preAborted }),
    (reason) => reason === 'before end',
  );
  await writer.write('a');
  const pending = writer.write('b');
  const controller = new AbortController();
  const signaledEnd = writer.end({ signal: controller.signal });
  const completedEnd = writer.end();
  controller.abort('during end');
  await assert.rejects(signaledEnd, (reason) => reason === 'during end');

  callbacks.shift()();
  await pending;
  callbacks.shift()();
  await completedEnd;
}

async function testDirectWriteThrowRejects() {
  const writable = new EventEmitter();
  const reason = new Error('duck write failed');
  writable.write = () => { throw reason; };
  writable.end = () => {};
  writable.destroy = () => {};
  const writer = fromWritable(writable);

  await assert.rejects(writer.write('a'), (error) => error === reason);
  writer.fail();
}

// =============================================================================
// strict: allows one pending write when the buffer is full
// =============================================================================

async function testStrictRejectsWhenFull() {
  const callbacks = [];
  const chunks = [];
  const writable = new Writable({
    highWaterMark: 5,
    write(chunk, enc, cb) {
      chunks.push(Buffer.from(chunk));
      callbacks.push(cb);
    },
  });

  const writer = fromWritable(writable);

  // First write fills the buffer (5 bytes = hwm)
  await writer.write('12345');

  const pending = writer.write('more');
  await setImmediate();
  assert.deepStrictEqual(chunks.map((chunk) => chunk.toString()), ['12345']);

  // One operation is pending, so a further write violates strict policy.
  await assert.rejects(
    writer.write('overflow'),
    { code: 'ERR_INVALID_STATE' },
  );

  callbacks.shift()();
  await pending;
  assert.deepStrictEqual(
    chunks.map((chunk) => chunk.toString()), ['12345', 'more']);
  const ending = writer.end();
  callbacks.shift()();
  await ending;
}

// =============================================================================
// strict: allows one pending writev when the buffer is full
// =============================================================================

async function testStrictWritevRejectsWhenFull() {
  const callbacks = [];
  const chunks = [];
  const writable = new Writable({
    highWaterMark: 5,
    write(chunk, enc, cb) {
      chunks.push(Buffer.from(chunk));
      callbacks.push(cb);
    },
    writev(entries, cb) {
      for (const { chunk } of entries) chunks.push(Buffer.from(chunk));
      callbacks.push(cb);
    },
  });

  const writer = fromWritable(writable);

  // Fill buffer
  await writer.write('12345');

  const pending = writer.writev([
    new TextEncoder().encode('a'),
    new TextEncoder().encode('b'),
  ]);
  await setImmediate();
  assert.deepStrictEqual(chunks.map((chunk) => chunk.toString()), ['12345']);

  await assert.rejects(
    writer.writev([
      new TextEncoder().encode('overflow'),
    ]),
    { code: 'ERR_INVALID_STATE' },
  );

  callbacks.shift()();
  await pending;
  assert.deepStrictEqual(
    chunks.map((chunk) => chunk.toString()), ['12345', 'a', 'b']);
  const ending = writer.end();
  callbacks.shift()();
  await ending;
}

// =============================================================================
// drop-newest: silently discards when buffer is full
// =============================================================================

async function testDropNewestDiscards() {
  const chunks = [];
  const writable = new Writable({
    highWaterMark: 5,
    write(chunk, enc, cb) {
      chunks.push(Buffer.from(chunk));
      // Never call cb -- data stays buffered
    },
  });

  const writer = fromWritable(writable, { backpressure: 'drop-newest' });

  // First write fills the buffer
  await writer.write('12345');

  // Second write should be silently discarded (no reject, no block)
  await writer.write('dropped');

  // Only the first chunk was actually written to the writable
  assert.strictEqual(chunks.length, 1);
  assert.strictEqual(chunks[0].toString(), '12345');
}

// =============================================================================
// drop-newest: writev discards entire batch when full
// =============================================================================

async function testDropNewestWritevDiscards() {
  const chunks = [];
  const writable = new Writable({
    highWaterMark: 5,
    write(chunk, enc, cb) {
      chunks.push(Buffer.from(chunk));
      // Never call cb
    },
  });

  const writer = fromWritable(writable, { backpressure: 'drop-newest' });

  // Fill buffer
  await writer.write('12345');

  // Writev should discard entire batch
  await writer.writev([
    new TextEncoder().encode('a'),
    new TextEncoder().encode('b'),
  ]);

  assert.strictEqual(chunks.length, 1);
}

// =============================================================================
// drop-newest: still counts bytes from dropped writes
// =============================================================================

async function testDropNewestCountsBytes() {
  const writable = new Writable({
    highWaterMark: 5,
    write(chunk, enc, cb) {
      // Never call cb
    },
  });

  const writer = fromWritable(writable, { backpressure: 'drop-newest' });

  await writer.write('12345');  // 5 bytes, accepted
  await writer.write('67890');  // 5 bytes, dropped

  // canWrite should be false (buffer is full)
  assert.strictEqual(writer.canWrite, false);
}

// =============================================================================
// drop-oldest: throws on construction
// =============================================================================

function testDropOldestThrows() {
  const writable = new Writable({ write(chunk, enc, cb) { cb(); } });
  assert.throws(
    () => fromWritable(writable, { backpressure: 'drop-oldest' }),
    { code: 'ERR_INVALID_ARG_VALUE' },
  );
}

// =============================================================================
// Invalid backpressure value throws
// =============================================================================

function testInvalidBackpressureThrows() {
  const writable = new Writable({ write(chunk, enc, cb) { cb(); } });
  assert.throws(
    () => fromWritable(writable, { backpressure: 'invalid' }),
    { code: 'ERR_INVALID_ARG_VALUE' },
  );
}

// =============================================================================
// writev() corks and uncorks (block policy)
// =============================================================================

async function testWritev() {
  const chunks = [];
  const writable = new Writable({
    highWaterMark: 1024,
    write(chunk, encoding, cb) {
      chunks.push(Buffer.from(chunk));
      cb();
    },
    writev(entries, cb) {
      for (const { chunk } of entries) {
        chunks.push(Buffer.from(chunk));
      }
      cb();
    },
  });

  const writer = fromWritable(writable, { backpressure: 'unbounded' });
  await writer.writev([
    new TextEncoder().encode('hello'),
    new TextEncoder().encode(' '),
    new TextEncoder().encode('world'),
  ]);
  await writer.end();

  assert.strictEqual(Buffer.concat(chunks).toString(), 'hello world');
}

// =============================================================================
// writeSync / writevSync always return false
// =============================================================================

function testSyncMethodsReturnFalse() {
  const writable = new Writable({ write(chunk, enc, cb) { cb(); } });
  const writer = fromWritable(writable);

  assert.strictEqual(writer.writeSync(new Uint8Array(1)), false);
  assert.strictEqual(writer.writevSync([new Uint8Array(1)]), false);
}

// =============================================================================
// endSync returns -1
// =============================================================================

function testEndSyncReturnsNegativeOne() {
  const writable = new Writable({ write(chunk, enc, cb) { cb(); } });
  const writer = fromWritable(writable);

  assert.strictEqual(writer.endSync(), -1);
}

// =============================================================================
// end() resolves with total bytes written
// =============================================================================

async function testEndReturnsByteCount() {
  const writable = new Writable({
    write(chunk, encoding, cb) { cb(); },
  });

  const writer = fromWritable(writable);
  await writer.write('hello');  // 5 bytes
  await writer.write(' world'); // 6 bytes
  const total = await writer.end();

  assert.strictEqual(total, 11);
}

// =============================================================================
// fail() destroys the writable
// =============================================================================

async function testFail() {
  const writable = new Writable({ write(chunk, enc, cb) { cb(); } });
  writable.on('error', common.mustCall());
  const writer = fromWritable(writable);

  writer.fail(new Error('test fail'));

  assert.ok(writable.destroyed);
}

// =============================================================================
// canWrite reflects buffer state
// =============================================================================

function testCanWrite() {
  const writable = new Writable({
    highWaterMark: 100,
    write(chunk, enc, cb) {
      // Don't call cb - keeps data buffered
    },
  });

  const writer = fromWritable(writable);
  assert.strictEqual(writer.canWrite, true);
}

// =============================================================================
// canWrite is null when destroyed
// =============================================================================

function testCanWriteNull() {
  const writable = new Writable({ write(chunk, enc, cb) { cb(); } });
  const writer = fromWritable(writable);

  writable.destroy();
  assert.strictEqual(writer.canWrite, null);
}

// =============================================================================
// drainableProtocol: resolves immediately when no backpressure
// =============================================================================

async function testDrainableNoPressure() {
  const writable = new Writable({
    highWaterMark: 1024,
    write(chunk, enc, cb) { cb(); },
  });

  const writer = fromWritable(writable);
  const result = await ondrain(writer);
  assert.strictEqual(result, true);
}

// =============================================================================
// drainableProtocol: returns null when destroyed
// =============================================================================

function testDrainableNull() {
  const writable = new Writable({ write(chunk, enc, cb) { cb(); } });
  const writer = fromWritable(writable);

  writable.destroy();
  assert.strictEqual(ondrain(writer), null);
}

// =============================================================================
// Error propagation: write after end rejects
// =============================================================================

async function testWriteAfterEnd() {
  const writable = new Writable({ write(chunk, enc, cb) { cb(); } });
  const writer = fromWritable(writable);

  await writer.end();

  await assert.rejects(
    writer.write('should fail'),
    { code: 'ERR_STREAM_WRITE_AFTER_END' },
  );
}

// =============================================================================
// Multiple sequential writes
// =============================================================================

async function testSequentialWrites() {
  const chunks = [];
  const writable = new Writable({
    write(chunk, encoding, cb) {
      chunks.push(Buffer.from(chunk));
      cb();
    },
  });

  const writer = fromWritable(writable);

  for (let i = 0; i < 10; i++) {
    await writer.write(`chunk${i}`);
  }
  await writer.end();

  let expected = '';
  for (let i = 0; i < 10; i++) {
    expected += `chunk${i}`;
  }
  assert.strictEqual(Buffer.concat(chunks).toString(), expected);
}

// =============================================================================
// pipeTo with compression transform into writable (block policy)
// =============================================================================

async function testPipeToWithTransform() {
  const {
    compressGzip,
    decompressGzip,
  } = require('zlib/iter');
  const { pull } = require('stream/iter');

  const compressed = [];
  const writable = new Writable({
    write(chunk, encoding, cb) {
      compressed.push(Buffer.from(chunk));
      cb();
    },
  });

  const writer = fromWritable(writable, { backpressure: 'unbounded' });
  await pipeTo(from('hello via transform'), compressGzip(), writer);

  const decompressed = await text(
    pull(from(Buffer.concat(compressed)), decompressGzip()),
  );
  assert.strictEqual(decompressed, 'hello via transform');
}

// =============================================================================
// Dispose support
// =============================================================================

async function testDispose() {
  const writable = new Writable({ write(chunk, enc, cb) { cb(); } });
  writable.on('error', common.mustCall());
  const writer = fromWritable(writable);

  writer[Symbol.dispose]();
  assert.ok(writable.destroyed);
}

async function testAsyncDispose() {
  const writable = new Writable({ write(chunk, enc, cb) { cb(); } });
  writable.on('error', common.mustCall());
  const writer = fromWritable(writable);

  await writer[Symbol.asyncDispose]();
  assert.ok(writable.destroyed);
}

// =============================================================================
// write() rejects values that cannot be converted to USVString
// =============================================================================

function testWriteInvalidChunkType() {
  const writable = new Writable({ write(chunk, enc, cb) { cb(); } });
  const writer = fromWritable(writable);

  assert.throws(
    () => writer.write(Symbol('invalid')),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
  writable.destroy();
}

// =============================================================================
// writev() validates chunks is an array
// =============================================================================

function testWritevInvalidChunksType() {
  const writable = new Writable({ write(chunk, enc, cb) { cb(); } });
  const writer = fromWritable(writable);

  assert.throws(
    () => writer.writev('not an array'),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
  assert.throws(
    () => writer.writev(42),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
}

// =============================================================================
// writev() uncorks when chunk validation throws
// =============================================================================

function testWritevInvalidChunkUncorks() {
  let writes = 0;
  const writable = new Writable({
    write(chunk, enc, cb) {
      writes++;
      cb();
    },
  });
  const writer = fromWritable(writable);

  assert.throws(
    () => writer.writev([new Uint8Array([1]), Symbol('invalid')]),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
  assert.strictEqual(writable.writableCorked, 0);
  assert.strictEqual(writes, 0);
}

// =============================================================================
// Cached writer: second call returns same instance
// =============================================================================

function testCachedWriter() {
  const writable = new Writable({ write(chunk, enc, cb) { cb(); } });
  const writer1 = fromWritable(writable);
  const writer2 = fromWritable(writable);

  assert.strictEqual(writer1, writer2);
}

// =============================================================================
// fail() rejects pending block waiters
// =============================================================================

async function testFailRejectsPendingWaiters() {
  const writable = new Writable({
    highWaterMark: 1,
    write(chunk, enc, cb) {
      // Never call cb -- stuck
    },
  });
  writable.on('error', common.mustCall());

  const writer = fromWritable(writable, { backpressure: 'unbounded' });

  await writer.write('a');
  const writePromise = writer.write('blocked data');

  // fail() should reject the pending waiter, not orphan it
  writer.fail(new Error('fail reason'));

  await assert.rejects(writePromise, { message: 'fail reason' });
}

async function testFailPreservesReason() {
  let classicError;
  const writable = new Writable({
    highWaterMark: 1,
    write() {},
  });
  writable.on('error', common.mustCall((error) => { classicError = error; }));
  const writer = fromWritable(writable, { backpressure: 'unbounded' });
  await writer.write('a');
  const pending = writer.write('blocked data');
  const draining = ondrain(writer);

  writer.fail(null);

  await assert.rejects(pending, (reason) => reason === null);
  await assert.rejects(draining, (reason) => reason === null);
  await assert.rejects(writer.write('more'), (reason) => reason === null);
  await assert.rejects(writer.end(), (reason) => reason === null);
  await setImmediate();
  assert.strictEqual(classicError.code, 'ERR_FALSY_VALUE_REJECTION');
  assert.strictEqual(classicError.reason, null);
}

async function testEndThrowPreservesReason() {
  const reason = undefined;
  const writable = new Writable({
    write(chunk, encoding, callback) { callback(); },
  });
  writable.on('error', common.mustCall());
  writable.end = () => { throw reason; };
  const writer = fromWritable(writable);

  await assert.rejects(writer.end(), (error) => error === reason);
  await assert.rejects(writer.write('more'), (error) => error === reason);
}

async function testFailWhileClosingPreservesReason() {
  let finish;
  const writable = new Writable({
    write(chunk, encoding, callback) { callback(); },
    final(callback) { finish = callback; },
  });
  writable.on('error', common.mustCall());
  const writer = fromWritable(writable);
  const ending = writer.end();

  writer.fail(false);

  await assert.rejects(ending, (reason) => reason === false);
  await assert.rejects(writer.write('more'), (reason) => reason === false);
  finish();
}

// =============================================================================
// dispose rejects pending block waiters
// =============================================================================

async function testDisposeRejectsPendingWaiters() {
  const writable = new Writable({
    highWaterMark: 1,
    write(chunk, enc, cb) {
      // Never call cb -- stuck
    },
  });
  writable.on('error', common.mustCall());

  const writer = fromWritable(writable, { backpressure: 'unbounded' });

  await writer.write('a');
  const writePromise = writer.write('blocked data');

  writer[Symbol.dispose]();

  await assert.rejects(writePromise, (reason) => reason === undefined);
}

// =============================================================================
// Run all tests
// =============================================================================

testFunctionExists();
testSyncMethodsReturnFalse();
// =============================================================================
// Object-mode Writable throws
// =============================================================================

function testObjectModeThrows() {
  const writable = new Writable({
    objectMode: true,
    write(chunk, enc, cb) { cb(); },
  });
  assert.throws(
    () => fromWritable(writable),
    { code: 'ERR_INVALID_STATE' },
  );
}

testFunctionExists();
testSyncMethodsReturnFalse();
testEndSyncReturnsNegativeOne();
testCanWrite();
testCanWriteNull();
testDrainableNull();
testDropOldestThrows();
testInvalidBackpressureThrows();
testWritevInvalidChunksType();
testWritevInvalidChunkUncorks();
testCachedWriter();
testObjectModeThrows();

Promise.all([
  testDefaultIsStrict(),
  testBasicWrite(),
  testWriteNoDrain(),
  testBlockWaitsForDrain(),
  testBlockErrorRejectsPendingWrite(),
  testErrorBeforeBackpressureIsStored(),
  testAlreadyErroredWritablePreservesReason(),
  testCleanDestroyRejectsQueuedOperations(),
  testPreAbortedWriteSignalsDoNotCommit(),
  testPendingWriteSignalRemovesOperation(),
  testZeroHighWaterMarkAcceptsFirstWrite(),
  testDuckWritableLatchesWriteBackpressure(),
  testPendingWritesRemainFifoDuringDrain(),
  testOndrainWaitsForEntirePendingQueue(),
  testEndSignal(),
  testDirectWriteThrowRejects(),
  testStrictRejectsWhenFull(),
  testStrictWritevRejectsWhenFull(),
  testDropNewestDiscards(),
  testDropNewestWritevDiscards(),
  testDropNewestCountsBytes(),
  testWritev(),
  testEndReturnsByteCount(),
  testFail(),
  testDrainableNoPressure(),
  testWriteAfterEnd(),
  testSequentialWrites(),
  testPipeToWithTransform(),
  testDispose(),
  testAsyncDispose(),
  testWriteInvalidChunkType(),
  testFailRejectsPendingWaiters(),
  testFailPreservesReason(),
  testFailWhileClosingPreservesReason(),
  testEndThrowPreservesReason(),
  testDisposeRejectsPendingWaiters(),
]).then(common.mustCall());
