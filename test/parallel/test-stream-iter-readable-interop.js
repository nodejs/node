// Flags: --experimental-stream-iter
'use strict';

// Tests for classic Readable stream interop with the stream/iter API
// via the toAsyncStreamable protocol and kValidatedSource optimization.

const common = require('../common');
const assert = require('assert');
const { Readable } = require('stream');
const {
  from,
  pull,
  bytes,
  text,
} = require('stream/iter');

const toAsyncStreamable = Symbol.for('Stream.toAsyncStreamable');

// =============================================================================
// toAsyncStreamable protocol is present on Readable.prototype
// =============================================================================

function testProtocolExists() {
  assert.strictEqual(typeof Readable.prototype[toAsyncStreamable], 'function');

  const readable = new Readable({ read() {} });
  assert.strictEqual(typeof readable[toAsyncStreamable], 'function');
}

// =============================================================================
// Byte-mode Readable: basic round-trip through from()
// =============================================================================

async function testByteModeThroughFrom() {
  const readable = new Readable({
    read() {
      this.push(Buffer.from('hello'));
      this.push(Buffer.from(' world'));
      this.push(null);
    },
  });

  const result = await text(from(readable));
  assert.strictEqual(result, 'hello world');
}

// =============================================================================
// Byte-mode Readable: basic round-trip through pull()
// =============================================================================

async function testByteModeThroughPull() {
  const readable = new Readable({
    read() {
      this.push(Buffer.from('pull '));
      this.push(Buffer.from('test'));
      this.push(null);
    },
  });

  const result = await text(pull(readable));
  assert.strictEqual(result, 'pull test');
}

// =============================================================================
// Byte-mode Readable: bytes consumer
// =============================================================================

async function testByteModeBytes() {
  const data = Buffer.from('binary data here');
  const readable = new Readable({
    read() {
      this.push(data);
      this.push(null);
    },
  });

  const result = await bytes(from(readable));
  assert.deepStrictEqual(result, new Uint8Array(data));
}

// =============================================================================
// Byte-mode Readable: batching - multiple buffered chunks yield as one batch
// =============================================================================

async function testBatchingBehavior() {
  const readable = new Readable({
    read() {
      // Push multiple chunks synchronously so they all buffer
      for (let i = 0; i < 10; i++) {
        this.push(Buffer.from(`chunk${i}`));
      }
      this.push(null);
    },
  });

  const source = from(readable);
  const batches = [];
  for await (const batch of source) {
    batches.push(batch);
  }

  // All chunks were buffered synchronously, so they should come out
  // as fewer batches than individual chunks (ideally one batch).
  assert.ok(batches.length < 10,
            `Expected fewer batches than chunks, got ${batches.length}`);

  // Total data should be correct
  const allChunks = batches.flat();
  const combined = Buffer.concat(allChunks);
  let expected = '';
  for (let i = 0; i < 10; i++) {
    expected += `chunk${i}`;
  }
  assert.strictEqual(combined.toString(), expected);
}

// =============================================================================
// Byte-mode Readable: kValidatedSource is set
// =============================================================================

function testTrustedSourceByteMode() {
  const readable = new Readable({ read() {} });
  const result = readable[toAsyncStreamable]();
  // kValidatedSource is a private symbol, but we can verify the result
  // is used directly by from() without wrapping by checking it has
  // Symbol.asyncIterator
  assert.strictEqual(typeof result[Symbol.asyncIterator], 'function');
  assert.strictEqual(result.stream, readable);
}

// =============================================================================
// Byte-mode Readable: multi-read with delayed pushes
// =============================================================================

async function testDelayedPushes() {
  let pushCount = 0;
  const readable = new Readable({
    read() {
      if (pushCount < 3) {
        setTimeout(() => {
          this.push(Buffer.from(`delayed${pushCount}`));
          pushCount++;
          if (pushCount === 3) {
            this.push(null);
          }
        }, 10);
      }
    },
  });

  const result = await text(from(readable));
  assert.strictEqual(result, 'delayed0delayed1delayed2');
}

// =============================================================================
// Byte-mode Readable: empty stream
// =============================================================================

async function testEmptyStream() {
  const readable = new Readable({
    read() {
      this.push(null);
    },
  });

  const result = await text(from(readable));
  assert.strictEqual(result, '');
}

// =============================================================================
// Byte-mode Readable: error propagation
// =============================================================================

async function testErrorPropagation() {
  const readable = new Readable({
    read() {
      process.nextTick(() => this.destroy(new Error('test error')));
    },
  });

  await assert.rejects(
    text(from(readable)),
    (err) => err.message === 'test error',
  );
}

// =============================================================================
// Byte-mode Readable: with transforms
// =============================================================================

async function testWithTransform() {
  const readable = new Readable({
    read() {
      this.push(Buffer.from('hello'));
      this.push(null);
    },
  });

  // Uppercase transform
  function uppercase(chunks) {
    if (chunks === null) return null;
    return chunks.map((c) => {
      const buf = Buffer.from(c);
      for (let i = 0; i < buf.length; i++) {
        if (buf[i] >= 97 && buf[i] <= 122) buf[i] -= 32;
      }
      return buf;
    });
  }

  const result = await text(pull(readable, uppercase));
  assert.strictEqual(result, 'HELLO');
}

// =============================================================================
// Object-mode Readable: strings are normalized to Uint8Array
// =============================================================================

async function testObjectModeStrings() {
  const readable = new Readable({
    objectMode: true,
    read() {
      this.push('hello');
      this.push(' object');
      this.push(' mode');
      this.push(null);
    },
  });

  const result = await text(from(readable));
  assert.strictEqual(result, 'hello object mode');
}

// =============================================================================
// Object-mode Readable: Uint8Array chunks pass through
// =============================================================================

async function testObjectModeUint8Array() {
  const readable = new Readable({
    objectMode: true,
    read() {
      this.push(new Uint8Array([72, 73]));  // "HI"
      this.push(null);
    },
  });

  const result = await text(from(readable));
  assert.strictEqual(result, 'HI');
}

// =============================================================================
// Object-mode Readable: mixed types (strings + Uint8Array)
// =============================================================================

async function testObjectModeMixed() {
  const readable = new Readable({
    objectMode: true,
    read() {
      this.push('hello');
      this.push(Buffer.from(' '));
      this.push('world');
      this.push(null);
    },
  });

  const result = await text(from(readable));
  assert.strictEqual(result, 'hello world');
}

// =============================================================================
// Object-mode Readable: toStreamable protocol objects
// =============================================================================

async function testObjectModeToStreamable() {
  const toStreamableSym = Symbol.for('Stream.toStreamable');
  const readable = new Readable({
    objectMode: true,
    read() {
      this.push({
        [toStreamableSym]() {
          return 'from-protocol';
        },
      });
      this.push(null);
    },
  });

  const result = await text(from(readable));
  assert.strictEqual(result, 'from-protocol');
}

// =============================================================================
// Object-mode Readable: kValidatedSource is set
// =============================================================================

function testTrustedSourceObjectMode() {
  const readable = new Readable({ objectMode: true, read() {} });
  const result = readable[toAsyncStreamable]();
  assert.strictEqual(typeof result[Symbol.asyncIterator], 'function');
  assert.strictEqual(result.stream, readable);
}

// =============================================================================
// Encoded Readable: strings are re-encoded to Uint8Array
// =============================================================================

async function testEncodedReadable() {
  const readable = new Readable({
    encoding: 'utf8',
    read() {
      this.push(Buffer.from('encoded'));
      this.push(null);
    },
  });

  const result = await text(from(readable));
  assert.strictEqual(result, 'encoded');
}

// =============================================================================
// Readable.from() source: verify interop with Readable.from()
// =============================================================================

async function testReadableFrom() {
  const readable = Readable.from(['chunk1', 'chunk2', 'chunk3']);

  const result = await text(from(readable));
  assert.strictEqual(result, 'chunk1chunk2chunk3');
}

// =============================================================================
// Byte-mode Readable: large data
// =============================================================================

async function testLargeData() {
  const totalSize = 1024 * 1024;  // 1 MB
  const chunkSize = 16384;
  let pushed = 0;

  const readable = new Readable({
    read() {
      if (pushed < totalSize) {
        const size = Math.min(chunkSize, totalSize - pushed);
        const buf = Buffer.alloc(size, 0x41);  // Fill with 'A'
        this.push(buf);
        pushed += size;
      } else {
        this.push(null);
      }
    },
  });

  const result = await bytes(from(readable));
  assert.strictEqual(result.length, totalSize);
  assert.strictEqual(result[0], 0x41);
  assert.strictEqual(result[totalSize - 1], 0x41);
}

// =============================================================================
// Byte-mode Readable: consumer return (early termination)
// =============================================================================

async function testEarlyTermination() {
  let pushCount = 0;
  const readable = new Readable({
    read() {
      this.push(Buffer.from(`chunk${pushCount++}`));
      // Never pushes null - infinite stream
    },
  });

  // Take only the first batch
  const source = from(readable);
  const batches = [];
  for await (const batch of source) {
    batches.push(batch);
    break;  // Stop after first batch
  }

  assert.ok(batches.length >= 1);
  // Stream should be destroyed after consumer return
  // Give it a tick to clean up
  await new Promise((resolve) => setTimeout(resolve, 50));
  assert.ok(readable.destroyed);
}

// =============================================================================
// Byte-mode Readable: pull() with compression transform
// =============================================================================

async function testWithCompression() {
  const {
    compressGzip,
    decompressGzip,
  } = require('zlib/iter');

  const readable = new Readable({
    read() {
      this.push(Buffer.from('compress me via classic Readable'));
      this.push(null);
    },
  });

  const compressed = pull(readable, compressGzip());
  const result = await text(pull(compressed, decompressGzip()));
  assert.strictEqual(result, 'compress me via classic Readable');
}

// =============================================================================
// Object-mode Readable: error propagation
// =============================================================================

async function testObjectModeError() {
  const readable = new Readable({
    objectMode: true,
    read() {
      process.nextTick(() => this.destroy(new Error('object error')));
    },
  });

  await assert.rejects(
    text(from(readable)),
    (err) => err.message === 'object error',
  );
}

// =============================================================================
// Stream destroyed mid-iteration
// =============================================================================

async function testDestroyMidIteration() {
  let pushCount = 0;
  const readable = new Readable({
    read() {
      this.push(Buffer.from(`chunk${pushCount++}`));
      // Never pushes null - infinite stream
    },
  });

  const chunks = [];
  await assert.rejects(async () => {
    for await (const batch of from(readable)) {
      chunks.push(...batch);
      if (chunks.length >= 3) {
        readable.destroy();
      }
    }
  }, { code: 'ERR_STREAM_PREMATURE_CLOSE' });
  assert.ok(readable.destroyed);
  assert.ok(chunks.length >= 3);
}

// =============================================================================
// Error after partial data
// =============================================================================

async function testErrorAfterPartialData() {
  let count = 0;
  const readable = new Readable({
    read() {
      if (count < 3) {
        this.push(Buffer.from(`ok${count++}`));
      } else {
        this.destroy(new Error('late error'));
      }
    },
  });

  const chunks = [];
  await assert.rejects(async () => {
    for await (const batch of from(readable)) {
      chunks.push(...batch);
    }
  }, { message: 'late error' });
  assert.ok(chunks.length > 0, 'Should have received partial data');
}

// =============================================================================
// Multiple consumers (second iteration yields empty)
// =============================================================================

async function testMultipleConsumers() {
  const readable = new Readable({
    read() {
      this.push(Buffer.from('data'));
      this.push(null);
    },
  });

  const first = await text(from(readable));
  assert.strictEqual(first, 'data');

  // Second consumption - stream is already consumed/destroyed
  const second = await text(from(readable));
  assert.strictEqual(second, '');
}

// =============================================================================
// highWaterMark: 0 - each chunk becomes its own batch
// =============================================================================

async function testHighWaterMarkZero() {
  let pushCount = 0;
  const readable = new Readable({
    highWaterMark: 0,
    read() {
      if (pushCount < 3) {
        this.push(Buffer.from(`hwm0-${pushCount++}`));
      } else {
        this.push(null);
      }
    },
  });

  const batches = [];
  for await (const batch of from(readable)) {
    batches.push(batch);
  }

  // With HWM=0, buffer is always empty so drain loop never fires.
  // Each chunk should be its own batch.
  assert.strictEqual(batches.length, 3);
  const combined = Buffer.concat(batches.flat());
  assert.strictEqual(combined.toString(), 'hwm0-0hwm0-1hwm0-2');
}

// =============================================================================
// Duplex stream (Duplex extends Readable, toAsyncStreamable should work)
// =============================================================================

async function testDuplexStream() {
  const { Duplex } = require('stream');

  const duplex = new Duplex({
    read() {
      this.push(Buffer.from('duplex-data'));
      this.push(null);
    },
    write(chunk, enc, cb) { cb(); },
  });

  const result = await text(from(duplex));
  assert.strictEqual(result, 'duplex-data');
}

// =============================================================================
// setEncoding called dynamically after construction
// =============================================================================

async function testSetEncodingDynamic() {
  const readable = new Readable({
    read() {
      this.push(Buffer.from('dynamic-enc'));
      this.push(null);
    },
  });

  readable.setEncoding('utf8');

  const result = await text(from(readable));
  assert.strictEqual(result, 'dynamic-enc');
}

// =============================================================================
// AbortSignal cancellation
// =============================================================================

async function testAbortSignal() {
  let pushCount = 0;
  const readable = new Readable({
    read() {
      this.push(Buffer.from(`sig${pushCount++}`));
      // Never pushes null - infinite stream
    },
  });

  const ac = new AbortController();
  const chunks = [];

  await assert.rejects(async () => {
    for await (const batch of pull(readable, { signal: ac.signal })) {
      chunks.push(...batch);
      if (chunks.length >= 2) {
        ac.abort();
      }
    }
  }, { name: 'AbortError' });
  assert.ok(chunks.length >= 2);
}

// =============================================================================
// kValidatedSource identity - from() returns same object for validated sources
// =============================================================================

function testTrustedSourceIdentity() {
  const readable = new Readable({ read() {} });
  const iter = readable[toAsyncStreamable]();

  // from() should return the validated iterator directly (same reference),
  // not wrap it in another generator
  const result = from(iter);
  assert.strictEqual(result, iter);
}

// =============================================================================
// Run all tests
// =============================================================================

testProtocolExists();
testTrustedSourceByteMode();
testTrustedSourceObjectMode();
testTrustedSourceIdentity();

Promise.all([
  testByteModeThroughFrom(),
  testByteModeThroughPull(),
  testByteModeBytes(),
  testBatchingBehavior(),
  testDelayedPushes(),
  testEmptyStream(),
  testErrorPropagation(),
  testWithTransform(),
  testObjectModeStrings(),
  testObjectModeUint8Array(),
  testObjectModeMixed(),
  testObjectModeToStreamable(),
  testEncodedReadable(),
  testReadableFrom(),
  testLargeData(),
  testEarlyTermination(),
  testWithCompression(),
  testObjectModeError(),
  testDestroyMidIteration(),
  testErrorAfterPartialData(),
  testMultipleConsumers(),
  testHighWaterMarkZero(),
  testDuplexStream(),
  testSetEncodingDynamic(),
  testAbortSignal(),
]).then(common.mustCall());
