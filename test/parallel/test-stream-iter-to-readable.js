// Flags: --experimental-stream-iter
'use strict';

// Tests for toReadable() and toReadableSync() which create byte-mode
// Readable streams from stream/iter sources.

const common = require('../common');
const assert = require('assert');
const { Writable } = require('stream');
const {
  from,
  fromSync,
  pull,
  text,
  toReadable,
  toReadableSync,
} = require('stream/iter');

function collect(readable) {
  return new Promise((resolve, reject) => {
    const chunks = [];
    readable.on('data', (chunk) => chunks.push(chunk));
    readable.on('end', () => resolve(Buffer.concat(chunks)));
    readable.on('error', reject);
  });
}

// =============================================================================
// fromStreamIter: basic async round-trip
// =============================================================================

async function testBasicAsync() {
  const source = from('hello world');
  const readable = toReadable(source);

  assert.strictEqual(readable.readableObjectMode, false);

  const result = await collect(readable);
  assert.strictEqual(result.toString(), 'hello world');
}

// =============================================================================
// fromStreamIter: multiple batches, all chunks arrive in order
// =============================================================================

async function testMultiBatchAsync() {
  async function* gen() {
    for (let i = 0; i < 5; i++) {
      const batch = [];
      for (let j = 0; j < 3; j++) {
        batch.push(Buffer.from(`${i}-${j} `));
      }
      yield batch;
    }
  }

  const readable = toReadable(gen());
  const result = await collect(readable);
  assert.strictEqual(result.toString(),
                     '0-0 0-1 0-2 1-0 1-1 1-2 2-0 2-1 2-2 3-0 3-1 3-2 4-0 4-1 4-2 ');
}

// =============================================================================
// fromStreamIter: backpressure with small highWaterMark
// =============================================================================

async function testBackpressureAsync() {
  let batchCount = 0;
  async function* gen() {
    for (let i = 0; i < 10; i++) {
      batchCount++;
      yield [Buffer.from(`chunk${i}`)];
    }
  }

  const readable = toReadable(gen(), { highWaterMark: 1 });

  // Read one chunk at a time with delays to exercise backpressure
  const chunks = [];
  for await (const chunk of readable) {
    chunks.push(chunk);
  }

  assert.strictEqual(chunks.length, 10);
  assert.strictEqual(batchCount, 10);
}

// =============================================================================
// fromStreamIter: source error mid-stream
// =============================================================================

async function testErrorAsync() {
  async function* gen() {
    yield [Buffer.from('ok')];
    throw new Error('source failed');
  }

  const readable = toReadable(gen());

  await assert.rejects(async () => {
    // eslint-disable-next-line no-unused-vars
    for await (const chunk of readable) {
      // Consume until error
    }
  }, { message: 'source failed' });
}

// =============================================================================
// fromStreamIter: empty source
// =============================================================================

async function testEmptyAsync() {
  async function* gen() {
    // yields nothing
  }

  const readable = toReadable(gen());
  const result = await collect(readable);
  assert.strictEqual(result.length, 0);
}

// =============================================================================
// fromStreamIter: empty batches are skipped
// =============================================================================

async function testEmptyBatchAsync() {
  async function* gen() {
    yield [];
    yield [Buffer.from('real data')];
    yield [];
  }

  const readable = toReadable(gen());
  const result = await collect(readable);
  assert.strictEqual(result.toString(), 'real data');
}

// =============================================================================
// fromStreamIter: destroy mid-stream cleans up iterator
// =============================================================================

async function testDestroyAsync() {
  let returnCalled = false;
  async function* gen() {
    try {
      for (let i = 0; ; i++) {
        yield [Buffer.from(`chunk${i}`)];
      }
    } finally {
      returnCalled = true;
    }
  }

  const readable = toReadable(gen());

  // Read a couple chunks then destroy
  const chunks = [];
  await new Promise((resolve, reject) => {
    readable.on('data', (chunk) => {
      chunks.push(chunk);
      if (chunks.length >= 3) {
        readable.destroy();
      }
    });
    readable.on('close', resolve);
    readable.on('error', reject);
  });

  assert.ok(chunks.length >= 3);
  assert.ok(returnCalled, 'iterator.return() should have been called');
}

// =============================================================================
// fromStreamIter: destroy while pump is waiting on backpressure
// =============================================================================

async function testDestroyDuringBackpressure() {
  let returnCalled = false;
  async function* gen() {
    try {
      // Yield a large batch that will trigger backpressure with HWM=1
      yield [Buffer.from('a'), Buffer.from('b'), Buffer.from('c')];
      // This should never be reached
      yield [Buffer.from('d')];
    } finally {
      returnCalled = true;
    }
  }

  const readable = toReadable(gen(), { highWaterMark: 1 });

  // Read one chunk to start the pump, then destroy while it's waiting
  const chunk = await new Promise((resolve) => {
    readable.once('readable', () => resolve(readable.read()));
  });
  assert.ok(chunk);

  // The pump should be waiting on backpressure now. Destroy the stream.
  readable.destroy();

  await new Promise((resolve) => readable.on('close', resolve));
  assert.ok(readable.destroyed);
  assert.ok(returnCalled, 'iterator.return() should have been called');
}

// =============================================================================
// fromStreamIter: large data integrity
// =============================================================================

async function testLargeDataAsync() {
  const totalSize = 1024 * 1024;  // 1 MB
  const chunkSize = 16384;

  async function* gen() {
    let remaining = totalSize;
    while (remaining > 0) {
      const size = Math.min(chunkSize, remaining);
      yield [Buffer.alloc(size, 0x42)];  // Fill with 'B'
      remaining -= size;
    }
  }

  const readable = toReadable(gen());
  const result = await collect(readable);
  assert.strictEqual(result.length, totalSize);
  assert.strictEqual(result[0], 0x42);
  assert.strictEqual(result[totalSize - 1], 0x42);
}

// =============================================================================
// fromStreamIter: pipe to Writable
// =============================================================================

async function testPipeAsync() {
  const source = from('pipe test data');
  const readable = toReadable(source);

  const chunks = [];
  const writable = new Writable({
    write(chunk, enc, cb) {
      chunks.push(chunk);
      cb();
    },
  });

  await new Promise((resolve, reject) => {
    readable.pipe(writable);
    writable.on('finish', resolve);
    writable.on('error', reject);
  });

  assert.strictEqual(Buffer.concat(chunks).toString(), 'pipe test data');
}

// =============================================================================
// fromStreamIter: not object mode
// =============================================================================

function testNotObjectMode() {
  async function* gen() { yield [Buffer.from('x')]; }
  const readable = toReadable(gen());
  assert.strictEqual(readable.readableObjectMode, false);
}

// =============================================================================
// fromStreamIter: invalid source throws
// =============================================================================

function testInvalidSourceAsync() {
  assert.throws(() => toReadable(42),
                { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => toReadable('not iterable'),
                { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => toReadable(null),
                { code: 'ERR_INVALID_ARG_TYPE' });
}

// =============================================================================
// fromStreamIter: signal aborts the readable
// =============================================================================

async function testSignalAsync() {
  async function* gen() {
    for (let i = 0; ; i++) {
      yield [Buffer.from(`chunk${i}`)];
    }
  }

  const ac = new AbortController();
  const readable = toReadable(gen(), { signal: ac.signal });

  const chunks = [];
  await assert.rejects(async () => {
    for await (const chunk of readable) {
      chunks.push(chunk);
      if (chunks.length >= 3) {
        ac.abort();
      }
    }
  }, { name: 'AbortError' });
  assert.ok(chunks.length >= 3);
  assert.ok(readable.destroyed);
}

// =============================================================================
// fromStreamIter: signal already aborted
// =============================================================================

async function testSignalAlreadyAborted() {
  async function* gen() {
    yield [Buffer.from('should not reach')];
  }

  const ac = new AbortController();
  ac.abort();
  const readable = toReadable(gen(), { signal: ac.signal });

  await assert.rejects(async () => {
    // eslint-disable-next-line no-unused-vars
    for await (const chunk of readable) {
      // Should not receive any data
    }
  }, { name: 'AbortError' });
  assert.ok(readable.destroyed);
}

// =============================================================================
// fromStreamIter: options validation
// =============================================================================

function testOptionsValidationAsync() {
  async function* gen() { yield [Buffer.from('x')]; }

  // Options must be an object
  assert.throws(() => toReadable(gen(), 'bad'),
                { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => toReadable(gen(), 42),
                { code: 'ERR_INVALID_ARG_TYPE' });

  // highWaterMark must be a non-negative integer
  assert.throws(() => toReadable(gen(), { highWaterMark: -1 }),
                { code: 'ERR_OUT_OF_RANGE' });
  assert.throws(() => toReadable(gen(), { highWaterMark: 'big' }),
                { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => toReadable(gen(), { highWaterMark: 1.5 }),
                { code: 'ERR_OUT_OF_RANGE' });

  // Signal must be an AbortSignal
  assert.throws(() => toReadable(gen(), { signal: 'not a signal' }),
                { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => toReadable(gen(), { signal: 42 }),
                { code: 'ERR_INVALID_ARG_TYPE' });

  // Valid options should work
  const r = toReadable(gen(), { highWaterMark: 0 });
  assert.strictEqual(r.readableHighWaterMark, 0);
  r.destroy();

  // Default highWaterMark should be 16KB
  const r2 = toReadable(gen());
  assert.strictEqual(r2.readableHighWaterMark, 64 * 1024);
  r2.destroy();

  // Explicit undefined options should work (uses defaults)
  const r3 = toReadable(gen(), undefined);
  assert.strictEqual(r3.readableHighWaterMark, 64 * 1024);
  r3.destroy();
}

// =============================================================================
// fromStreamIter: with stream/iter transform
// =============================================================================

async function testWithTransformAsync() {
  // Uppercase transform
  function upper(chunks) {
    if (chunks === null) return null;
    return chunks.map((c) => {
      const buf = Buffer.from(c);
      for (let i = 0; i < buf.length; i++) {
        if (buf[i] >= 97 && buf[i] <= 122) buf[i] -= 32;
      }
      return buf;
    });
  }

  const source = pull(from('hello world'), upper);
  const readable = toReadable(source);
  const result = await collect(readable);
  assert.strictEqual(result.toString(), 'HELLO WORLD');
}

// =============================================================================
// fromStreamIterSync: basic sync round-trip
// =============================================================================

async function testBasicSync() {
  const source = fromSync('sync hello');
  const readable = toReadableSync(source);

  assert.strictEqual(readable.readableObjectMode, false);

  const result = await collect(readable);
  assert.strictEqual(result.toString(), 'sync hello');
}

// =============================================================================
// fromStreamIterSync: synchronous read() returns data immediately
// =============================================================================

function testSyncRead() {
  const source = fromSync('immediate');
  const readable = toReadableSync(source);

  // Synchronous read should return data right away
  const chunk = readable.read();
  assert.ok(Buffer.isBuffer(chunk));
  assert.strictEqual(chunk.toString(), 'immediate');
}

// =============================================================================
// fromStreamIterSync: backpressure with small highWaterMark
// =============================================================================

async function testBackpressureSync() {
  function* gen() {
    for (let i = 0; i < 10; i++) {
      yield [Buffer.from(`s${i}`)];
    }
  }

  const readable = toReadableSync(gen(), { highWaterMark: 1 });

  const chunks = [];
  for await (const chunk of readable) {
    chunks.push(chunk);
  }

  assert.strictEqual(chunks.length, 10);
}

// =============================================================================
// fromStreamIterSync: source error
// =============================================================================

async function testErrorSync() {
  function* gen() {
    yield [Buffer.from('ok')];
    throw new Error('sync source failed');
  }

  const readable = toReadableSync(gen());

  await assert.rejects(async () => {
    // eslint-disable-next-line no-unused-vars
    for await (const chunk of readable) {
      // Consume until error
    }
  }, { message: 'sync source failed' });
}

// =============================================================================
// fromStreamIterSync: empty source
// =============================================================================

function testEmptySync() {
  function* gen() {
    // yields nothing
  }

  const readable = toReadableSync(gen());
  const result = readable.read();
  assert.strictEqual(result, null);
}

// =============================================================================
// fromStreamIterSync: destroy calls iterator.return()
// =============================================================================

async function testDestroySync() {
  let returnCalled = false;
  function* gen() {
    try {
      for (let i = 0; ; i++) {
        yield [Buffer.from(`chunk${i}`)];
      }
    } finally {
      returnCalled = true;
    }
  }

  const readable = toReadableSync(gen());
  readable.read();  // Start iteration
  readable.destroy();

  await new Promise((resolve) => readable.on('close', resolve));
  assert.ok(returnCalled, 'iterator.return() should have been called');
}

// =============================================================================
// fromStreamIterSync: not object mode
// =============================================================================

function testNotObjectModeSync() {
  function* gen() { yield [Buffer.from('x')]; }
  const readable = toReadableSync(gen());
  assert.strictEqual(readable.readableObjectMode, false);
}

// =============================================================================
// fromStreamIterSync: invalid source throws
// =============================================================================

function testInvalidSourceSync() {
  assert.throws(() => toReadableSync(42),
                { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => toReadableSync(null),
                { code: 'ERR_INVALID_ARG_TYPE' });
}

// =============================================================================
// fromStreamIterSync: options validation
// =============================================================================

function testOptionsValidationSync() {
  function* gen() { yield [Buffer.from('x')]; }

  // Options must be an object
  assert.throws(() => toReadableSync(gen(), 'bad'),
                { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => toReadableSync(gen(), 42),
                { code: 'ERR_INVALID_ARG_TYPE' });

  // highWaterMark must be a non-negative integer
  assert.throws(() => toReadableSync(gen(), { highWaterMark: -1 }),
                { code: 'ERR_OUT_OF_RANGE' });
  assert.throws(() => toReadableSync(gen(), { highWaterMark: 'big' }),
                { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => toReadableSync(gen(), { highWaterMark: 1.5 }),
                { code: 'ERR_OUT_OF_RANGE' });

  // Valid options should work
  const r = toReadableSync(gen(), { highWaterMark: 0 });
  assert.strictEqual(r.readableHighWaterMark, 0);
  r.destroy();

  // Default highWaterMark should be 16KB
  const r2 = toReadableSync(gen());
  assert.strictEqual(r2.readableHighWaterMark, 64 * 1024);
  r2.destroy();
}

// =============================================================================
// Round-trip: stream/iter -> Readable -> stream/iter
// =============================================================================

async function testRoundTrip() {
  const original = 'round trip data test';

  // stream/iter -> classic Readable
  const source = from(original);
  const readable = toReadable(source);

  // classic Readable -> stream/iter (via toAsyncStreamable)
  const result = await text(from(readable));
  assert.strictEqual(result, original);
}

// =============================================================================
// Round-trip with compression
// =============================================================================

async function testRoundTripWithCompression() {
  const { compressGzip, decompressGzip } = require('zlib/iter');

  const original = 'compress through classic readable';

  // Compress via stream/iter, bridge to classic Readable
  const compressed = pull(from(original), compressGzip());
  const readable = toReadable(compressed);

  // Classic Readable back to stream/iter for decompression
  const result = await text(pull(from(readable), decompressGzip()));
  assert.strictEqual(result, original);
}

// =============================================================================
// Run all tests
// =============================================================================

testNotObjectMode();
testNotObjectModeSync();
testSyncRead();
testEmptySync();
testInvalidSourceAsync();
testInvalidSourceSync();
testOptionsValidationAsync();
testOptionsValidationSync();

Promise.all([
  testBasicAsync(),
  testMultiBatchAsync(),
  testBackpressureAsync(),
  testErrorAsync(),
  testEmptyAsync(),
  testEmptyBatchAsync(),
  testDestroyAsync(),
  testDestroyDuringBackpressure(),
  testSignalAsync(),
  testSignalAlreadyAborted(),
  testLargeDataAsync(),
  testPipeAsync(),
  testWithTransformAsync(),
  testBasicSync(),
  testBackpressureSync(),
  testErrorSync(),
  testDestroySync(),
  testRoundTrip(),
  testRoundTripWithCompression(),
]).then(common.mustCall());
