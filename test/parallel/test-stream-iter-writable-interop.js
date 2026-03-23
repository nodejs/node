// Flags: --experimental-stream-iter
'use strict';

// Tests for classic Writable stream interop with the stream/iter API
// via fromWritable().

const common = require('../common');
const assert = require('assert');
const { Writable } = require('stream');
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

  const writer = fromWritable(writable, { backpressure: 'block' });
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

  const writer = fromWritable(writable, { backpressure: 'block' });

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

  const writer = fromWritable(writable, { backpressure: 'block' });

  // First write fills the buffer, waits for drain
  const writePromise = writer.write('data that will block');

  // Destroy with error while write is pending
  writable.destroy(new Error('stream broke'));

  await assert.rejects(writePromise, { message: 'stream broke' });
}

// =============================================================================
// strict: rejects when buffer is full
// =============================================================================

async function testStrictRejectsWhenFull() {
  const writable = new Writable({
    highWaterMark: 5,
    write(chunk, enc, cb) {
      // Never call cb -- data stays buffered
    },
  });

  const writer = fromWritable(writable);

  // First write fills the buffer (5 bytes = hwm)
  await writer.write('12345');

  // Second write should reject -- buffer is full
  await assert.rejects(
    writer.write('more'),
    { code: 'ERR_INVALID_STATE' },
  );
}

// =============================================================================
// strict: writev rejects when buffer is full
// =============================================================================

async function testStrictWritevRejectsWhenFull() {
  const writable = new Writable({
    highWaterMark: 5,
    write(chunk, enc, cb) {
      // Never call cb
    },
  });

  const writer = fromWritable(writable);

  // Fill buffer
  await writer.write('12345');

  // Writev should reject entire batch
  await assert.rejects(
    writer.writev([
      new TextEncoder().encode('a'),
      new TextEncoder().encode('b'),
    ]),
    { code: 'ERR_INVALID_STATE' },
  );
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

  // desiredSize should be 0 (buffer is full)
  assert.strictEqual(writer.desiredSize, 0);
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

  const writer = fromWritable(writable, { backpressure: 'block' });
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
  writable.on('error', () => {});  // Prevent unhandled error
  const writer = fromWritable(writable);

  writer.fail(new Error('test fail'));

  assert.ok(writable.destroyed);
}

// =============================================================================
// desiredSize reflects buffer state
// =============================================================================

function testDesiredSize() {
  const writable = new Writable({
    highWaterMark: 100,
    write(chunk, enc, cb) {
      // Don't call cb - keeps data buffered
    },
  });

  const writer = fromWritable(writable);
  assert.strictEqual(writer.desiredSize, 100);
}

// =============================================================================
// desiredSize is null when destroyed
// =============================================================================

function testDesiredSizeNull() {
  const writable = new Writable({ write(chunk, enc, cb) { cb(); } });
  const writer = fromWritable(writable);

  writable.destroy();
  assert.strictEqual(writer.desiredSize, null);
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

  const writer = fromWritable(writable, { backpressure: 'block' });
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
  const writer = fromWritable(writable);

  writer[Symbol.dispose]();
  assert.ok(writable.destroyed);
}

async function testAsyncDispose() {
  const writable = new Writable({ write(chunk, enc, cb) { cb(); } });
  const writer = fromWritable(writable);

  await writer[Symbol.asyncDispose]();
  assert.ok(writable.destroyed);
}

// =============================================================================
// write() validates chunk type
// =============================================================================

async function testWriteInvalidChunkType() {
  const writable = new Writable({ write(chunk, enc, cb) { cb(); } });
  const writer = fromWritable(writable);

  await assert.rejects(
    writer.write(42),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
  await assert.rejects(
    writer.write(null),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
  await assert.rejects(
    writer.write({}),
    { code: 'ERR_INVALID_ARG_TYPE' },
  );
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
  writable.on('error', () => {});  // Prevent unhandled error

  const writer = fromWritable(writable, { backpressure: 'block' });

  // This write will block on drain
  const writePromise = writer.write('blocked data');

  // fail() should reject the pending waiter, not orphan it
  writer.fail(new Error('fail reason'));

  await assert.rejects(writePromise, { message: 'fail reason' });
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

  const writer = fromWritable(writable, { backpressure: 'block' });

  // This write will block on drain
  const writePromise = writer.write('blocked data');

  writer[Symbol.dispose]();

  await assert.rejects(writePromise, { name: 'AbortError' });
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
testDesiredSize();
testDesiredSizeNull();
testDrainableNull();
testDropOldestThrows();
testInvalidBackpressureThrows();
testWritevInvalidChunksType();
testCachedWriter();
testObjectModeThrows();

Promise.all([
  testDefaultIsStrict(),
  testBasicWrite(),
  testWriteNoDrain(),
  testBlockWaitsForDrain(),
  testBlockErrorRejectsPendingWrite(),
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
  testDisposeRejectsPendingWaiters(),
]).then(common.mustCall());
