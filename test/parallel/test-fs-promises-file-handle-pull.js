// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const { open } = fs.promises;
const path = require('path');
const tmpdir = require('../common/tmpdir');
const { text, bytes } = require('stream/iter');

tmpdir.refresh();

const tmpDir = tmpdir.path;

// =============================================================================
// Basic pull()
// =============================================================================

async function testBasicPull() {
  const filePath = path.join(tmpDir, 'pull-basic.txt');
  fs.writeFileSync(filePath, 'hello from file');

  const fh = await open(filePath, 'r');
  try {
    const readable = fh.pull();
    const data = await text(readable);
    assert.strictEqual(data, 'hello from file');
  } finally {
    await fh.close();
  }
}

async function testPullBinary() {
  const filePath = path.join(tmpDir, 'pull-binary.bin');
  const buf = Buffer.alloc(256);
  for (let i = 0; i < 256; i++) buf[i] = i;
  fs.writeFileSync(filePath, buf);

  const fh = await open(filePath, 'r');
  try {
    const readable = fh.pull();
    const data = await bytes(readable);
    assert.strictEqual(data.byteLength, 256);
    for (let i = 0; i < 256; i++) {
      assert.strictEqual(data[i], i);
    }
  } finally {
    await fh.close();
  }
}

async function testPullEmptyFile() {
  const filePath = path.join(tmpDir, 'pull-empty.txt');
  fs.writeFileSync(filePath, '');

  const fh = await open(filePath, 'r');
  try {
    const readable = fh.pull();
    const data = await bytes(readable);
    assert.strictEqual(data.byteLength, 0);
  } finally {
    await fh.close();
  }
}

// =============================================================================
// Large file (multi-chunk)
// =============================================================================

async function testPullLargeFile() {
  const filePath = path.join(tmpDir, 'pull-large.bin');
  // Write 64KB - enough for multiple 16KB read chunks
  const size = 64 * 1024;
  const buf = Buffer.alloc(size, 0x42);
  fs.writeFileSync(filePath, buf);

  const fh = await open(filePath, 'r');
  try {
    const readable = fh.pull();
    const data = await bytes(readable);
    assert.strictEqual(data.byteLength, size);
    // Verify content
    for (let i = 0; i < data.byteLength; i++) {
      assert.strictEqual(data[i], 0x42);
    }
  } finally {
    await fh.close();
  }
}

// =============================================================================
// With transforms
// =============================================================================

async function testPullWithTransform() {
  const filePath = path.join(tmpDir, 'pull-transform.txt');
  fs.writeFileSync(filePath, 'hello');

  const fh = await open(filePath, 'r');
  try {
    const upper = (chunks) => {
      if (chunks === null) return null;
      return chunks.map((c) => {
        const str = new TextDecoder().decode(c);
        return new TextEncoder().encode(str.toUpperCase());
      });
    };

    const readable = fh.pull(upper);
    const data = await text(readable);
    assert.strictEqual(data, 'HELLO');
  } finally {
    await fh.close();
  }
}

// =============================================================================
// autoClose option
// =============================================================================

async function testPullAutoClose() {
  const filePath = path.join(tmpDir, 'pull-autoclose.txt');
  fs.writeFileSync(filePath, 'auto close data');

  const fh = await open(filePath, 'r');
  const readable = fh.pull({ autoClose: true });
  const data = await text(readable);
  assert.strictEqual(data, 'auto close data');

  // After consuming with autoClose, the file handle should be closed
  // Trying to read again should throw
  await assert.rejects(
    async () => {
      await fh.stat();
    },
    (err) => err.code === 'ERR_INVALID_STATE' || err.code === 'EBADF',
  );
}

// =============================================================================
// Locking
// =============================================================================

async function testPullLocking() {
  const filePath = path.join(tmpDir, 'pull-lock.txt');
  fs.writeFileSync(filePath, 'lock data');

  const fh = await open(filePath, 'r');
  try {
    // First pull locks the handle
    const readable = fh.pull();

    // Second pull while locked should throw
    assert.throws(
      () => fh.pull(),
      { code: 'ERR_INVALID_STATE' },
    );

    // Consume the first stream to unlock
    await text(readable);

    // Now it should be usable again
    const readable2 = fh.pull();
    const data = await text(readable2);
    assert.strictEqual(data, '');  // Already read to end
  } finally {
    await fh.close();
  }
}

// =============================================================================
// Closed handle
// =============================================================================

async function testPullClosedHandle() {
  const filePath = path.join(tmpDir, 'pull-closed.txt');
  fs.writeFileSync(filePath, 'data');

  const fh = await open(filePath, 'r');
  await fh.close();

  assert.throws(
    () => fh.pull(),
    { code: 'ERR_INVALID_STATE' },
  );
}

// =============================================================================
// AbortSignal
// =============================================================================

async function testPullAbortSignal() {
  const filePath = path.join(tmpDir, 'pull-abort.txt');
  // Write enough data that we can abort mid-stream
  fs.writeFileSync(filePath, 'a'.repeat(1024));

  const ac = new AbortController();
  const fh = await open(filePath, 'r');
  try {
    ac.abort();
    const readable = fh.pull({ signal: ac.signal });

    await assert.rejects(
      async () => {
        // eslint-disable-next-line no-unused-vars
        for await (const _ of readable) {
          assert.fail('Should not reach here');
        }
      },
      (err) => err.name === 'AbortError',
    );
  } finally {
    await fh.close();
  }
}

// =============================================================================
// Iterate batches directly
// =============================================================================

async function testPullIterateBatches() {
  const filePath = path.join(tmpDir, 'pull-batches.txt');
  fs.writeFileSync(filePath, 'batch data');

  const fh = await open(filePath, 'r');
  try {
    const readable = fh.pull();
    const batches = [];
    for await (const batch of readable) {
      batches.push(batch);
      // Each batch should be an array of Uint8Array
      assert.ok(Array.isArray(batch));
      for (const chunk of batch) {
        assert.ok(chunk instanceof Uint8Array);
      }
    }
    assert.ok(batches.length > 0);
  } finally {
    await fh.close();
  }
}

// =============================================================================
// pull() with start option - read from specific position
// =============================================================================

async function testPullStart() {
  const filePath = path.join(tmpDir, 'pull-start.txt');
  fs.writeFileSync(filePath, 'AAABBBCCC');

  const fh = await open(filePath, 'r');
  try {
    // Read from offset 3
    const data = await text(fh.pull({ start: 3 }));
    assert.strictEqual(data, 'BBBCCC');
  } finally {
    await fh.close();
  }
}

// =============================================================================
// pull() with limit option - read at most N bytes
// =============================================================================

async function testPullLimit() {
  const filePath = path.join(tmpDir, 'pull-limit.txt');
  fs.writeFileSync(filePath, 'Hello, World! Extra data here.');

  const fh = await open(filePath, 'r');
  try {
    const data = await text(fh.pull({ limit: 13 }));
    assert.strictEqual(data, 'Hello, World!');
  } finally {
    await fh.close();
  }
}

// =============================================================================
// pull() with start + limit - read a slice
// =============================================================================

async function testPullStartAndLimit() {
  const filePath = path.join(tmpDir, 'pull-start-limit.txt');
  fs.writeFileSync(filePath, 'AAABBBCCCDDD');

  const fh = await open(filePath, 'r');
  try {
    // Read 3 bytes starting at offset 3
    const data = await text(fh.pull({ start: 3, limit: 3 }));
    assert.strictEqual(data, 'BBB');
  } finally {
    await fh.close();
  }
}

// =============================================================================
// pull() with limit larger than file - reads whole file
// =============================================================================

async function testPullLimitLargerThanFile() {
  const filePath = path.join(tmpDir, 'pull-limit-large.txt');
  fs.writeFileSync(filePath, 'short');

  const fh = await open(filePath, 'r');
  try {
    const data = await text(fh.pull({ limit: 1000000 }));
    assert.strictEqual(data, 'short');
  } finally {
    await fh.close();
  }
}

// =============================================================================
// pull() with limit spanning multiple chunks
// =============================================================================

async function testPullLimitMultiChunk() {
  const filePath = path.join(tmpDir, 'pull-limit-multi.bin');
  // 300KB file - spans multiple 128KB reads
  const input = Buffer.alloc(300 * 1024, 'x');
  fs.writeFileSync(filePath, input);

  const fh = await open(filePath, 'r');
  try {
    // Read exactly 200KB from offset 50KB
    const data = await bytes(fh.pull({ start: 50 * 1024, limit: 200 * 1024 }));
    assert.strictEqual(data.byteLength, 200 * 1024);
  } finally {
    await fh.close();
  }
}

// =============================================================================
// pull() with start + limit + transforms
// =============================================================================

async function testPullStartLimitWithTransforms() {
  const filePath = path.join(tmpDir, 'pull-start-limit-transform.txt');
  fs.writeFileSync(filePath, 'aaabbbcccddd');

  const fh = await open(filePath, 'r');
  try {
    const { compressGzip, decompressGzip } = require('zlib/iter');
    const compressed = fh.pull(compressGzip(), { start: 3, limit: 6 });
    const decompressed = await text(
      require('stream/iter').pull(compressed, decompressGzip()));
    assert.strictEqual(decompressed, 'bbbccc');
  } finally {
    await fh.close();
  }
}

// =============================================================================
// pull() with chunkSize option
// =============================================================================

async function testPullChunkSize() {
  const filePath = path.join(tmpDir, 'pull-chunksize.bin');
  // Write 64KB of data
  const input = Buffer.alloc(64 * 1024, 'z');
  fs.writeFileSync(filePath, input);

  const fh = await open(filePath, 'r');
  try {
    // Use 16KB chunks - should produce 4 batches
    let batchCount = 0;
    for await (const batch of fh.pull({ chunkSize: 16 * 1024 })) {
      batchCount++;
      for (const chunk of batch) {
        assert.ok(chunk.byteLength <= 16 * 1024,
                  `Chunk ${chunk.byteLength} should be <= 16384`);
      }
    }
    assert.strictEqual(batchCount, 4);
  } finally {
    await fh.close();
  }
}

async function testPullChunkSizeSmall() {
  const filePath = path.join(tmpDir, 'pull-chunksize-small.txt');
  fs.writeFileSync(filePath, 'hello');

  const fh = await open(filePath, 'r');
  try {
    // 1-byte chunks
    let totalBytes = 0;
    let batchCount = 0;
    for await (const batch of fh.pull({ chunkSize: 1 })) {
      batchCount++;
      for (const chunk of batch) totalBytes += chunk.byteLength;
    }
    assert.strictEqual(totalBytes, 5);
    assert.strictEqual(batchCount, 5);
  } finally {
    await fh.close();
  }
}

async function testPullSyncArgumentValidation() {
  const filePath = path.join(tmpDir, 'pull-arg-validation.txt');
  fs.writeFileSync(filePath, 'data');

  const fh = await open(filePath, 'r');
  try {
    assert.throws(() => fh.pull({ autoClose: 'no' }), { code: 'ERR_INVALID_ARG_TYPE' });
    assert.throws(() => fh.pull({ start: 'a' }), { code: 'ERR_INVALID_ARG_TYPE' });
    assert.throws(() => fh.pull({ limit: 'a' }), { code: 'ERR_INVALID_ARG_TYPE' });
    assert.throws(() => fh.pull({ chunkSize: 'a' }), { code: 'ERR_INVALID_ARG_TYPE' });
    assert.throws(() => fh.pull({ signal: {} }), { code: 'ERR_INVALID_ARG_TYPE' });
    assert.throws(() => fh.pull({ start: 1.1 }), { code: 'ERR_OUT_OF_RANGE' });
    assert.throws(() => fh.pull({ limit: 1.1 }), { code: 'ERR_OUT_OF_RANGE' });
    assert.throws(() => fh.pull({ chunkSize: 1.1 }), { code: 'ERR_OUT_OF_RANGE' });
  } finally {
    await fh.close();
  }
}

Promise.all([
  testBasicPull(),
  testPullBinary(),
  testPullEmptyFile(),
  testPullLargeFile(),
  testPullWithTransform(),
  testPullAutoClose(),
  testPullLocking(),
  testPullClosedHandle(),
  testPullAbortSignal(),
  testPullIterateBatches(),
  testPullStart(),
  testPullLimit(),
  testPullStartAndLimit(),
  testPullLimitLargerThanFile(),
  testPullLimitMultiChunk(),
  testPullStartLimitWithTransforms(),
  testPullChunkSize(),
  testPullChunkSizeSmall(),
  testPullSyncArgumentValidation(),
]).then(common.mustCall());
