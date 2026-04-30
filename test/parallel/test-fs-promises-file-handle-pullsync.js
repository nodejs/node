// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const { open } = fs.promises;
const path = require('path');
const tmpdir = require('../common/tmpdir');
const {
  textSync,
  bytesSync,
  pipeToSync,
  pullSync,
} = require('stream/iter');
const {
  compressGzipSync,
  decompressGzipSync,
} = require('zlib/iter');

tmpdir.refresh();

const tmpDir = tmpdir.path;

// =============================================================================
// Basic pullSync()
// =============================================================================

async function testBasicPullSync() {
  const filePath = path.join(tmpDir, 'pullsync-basic.txt');
  fs.writeFileSync(filePath, 'hello from sync file read');

  const fh = await open(filePath, 'r');
  try {
    const data = textSync(fh.pullSync());
    assert.strictEqual(data, 'hello from sync file read');
  } finally {
    await fh.close();
  }
}

// =============================================================================
// Large file (multi-chunk)
// =============================================================================

async function testLargeFile() {
  const filePath = path.join(tmpDir, 'pullsync-large.txt');
  const input = 'sync large data test. '.repeat(10000);
  fs.writeFileSync(filePath, input);

  const fh = await open(filePath, 'r');
  try {
    const data = textSync(fh.pullSync());
    assert.strictEqual(data, input);
  } finally {
    await fh.close();
  }
}

// =============================================================================
// Binary data round-trip
// =============================================================================

async function testBinaryData() {
  const filePath = path.join(tmpDir, 'pullsync-binary.bin');
  const input = Buffer.alloc(200000);
  for (let i = 0; i < input.length; i++) input[i] = i & 0xff;
  fs.writeFileSync(filePath, input);

  const fh = await open(filePath, 'r');
  try {
    const data = bytesSync(fh.pullSync());
    assert.deepStrictEqual(Buffer.from(data), input);
  } finally {
    await fh.close();
  }
}

// =============================================================================
// pullSync with sync compression transform round-trip
// =============================================================================

async function testPullSyncWithCompression() {
  const filePath = path.join(tmpDir, 'pullsync-compress-src.txt');
  const dstPath = path.join(tmpDir, 'pullsync-compress-dst.gz');
  const input = 'compress via sync pullSync. '.repeat(1000);
  fs.writeFileSync(filePath, input);

  // Compress: pullSync -> compressGzipSync -> write to file
  const srcFh = await open(filePath, 'r');
  const dstFh = await open(dstPath, 'w');
  try {
    const w = dstFh.writer();
    pipeToSync(srcFh.pullSync(compressGzipSync()), w);
  } finally {
    await srcFh.close();
    await dstFh.close();
  }

  // Verify compressed file is smaller
  const compressedSize = fs.statSync(dstPath).size;
  assert.ok(compressedSize < Buffer.byteLength(input),
            `Compressed ${compressedSize} should be < original ` +
            `${Buffer.byteLength(input)}`);

  // Decompress and verify
  const readFh = await open(dstPath, 'r');
  try {
    const result = textSync(readFh.pullSync(decompressGzipSync()));
    assert.strictEqual(result, input);
  } finally {
    await readFh.close();
  }
}

// =============================================================================
// pullSync with stateless transform
// =============================================================================

async function testPullSyncWithStatelessTransform() {
  const filePath = path.join(tmpDir, 'pullsync-upper.txt');
  fs.writeFileSync(filePath, 'hello world');

  const upper = (chunks) => {
    if (chunks === null) return null;
    const out = new Array(chunks.length);
    for (let j = 0; j < chunks.length; j++) {
      const src = chunks[j];
      const buf = Buffer.allocUnsafe(src.length);
      for (let i = 0; i < src.length; i++) {
        const b = src[i];
        buf[i] = (b >= 0x61 && b <= 0x7a) ? b - 0x20 : b;
      }
      out[j] = buf;
    }
    return out;
  };

  const fh = await open(filePath, 'r');
  try {
    const data = textSync(fh.pullSync(upper));
    assert.strictEqual(data, 'HELLO WORLD');
  } finally {
    await fh.close();
  }
}

// =============================================================================
// pullSync with mixed stateless + stateful transforms
// =============================================================================

async function testPullSyncMixedTransforms() {
  const filePath = path.join(tmpDir, 'pullsync-mixed.txt');
  const input = 'mixed transform test '.repeat(500);
  fs.writeFileSync(filePath, input);

  const upper = (chunks) => {
    if (chunks === null) return null;
    const out = new Array(chunks.length);
    for (let j = 0; j < chunks.length; j++) {
      const src = chunks[j];
      const buf = Buffer.allocUnsafe(src.length);
      for (let i = 0; i < src.length; i++) {
        const b = src[i];
        buf[i] = (b >= 0x61 && b <= 0x7a) ? b - 0x20 : b;
      }
      out[j] = buf;
    }
    return out;
  };

  const fh = await open(filePath, 'r');
  try {
    // Upper + compress + decompress
    const data = textSync(
      pullSync(fh.pullSync(upper, compressGzipSync()), decompressGzipSync()),
    );
    assert.strictEqual(data, input.toUpperCase());
  } finally {
    await fh.close();
  }
}

// =============================================================================
// autoClose: true - handle closed after iteration completes
// =============================================================================

async function testAutoClose() {
  const filePath = path.join(tmpDir, 'pullsync-autoclose.txt');
  fs.writeFileSync(filePath, 'auto close test');

  const fh = await open(filePath, 'r');
  const data = textSync(fh.pullSync({ autoClose: true }));
  assert.strictEqual(data, 'auto close test');

  // Handle should be closed
  await assert.rejects(fh.stat(), { code: 'EBADF' });
}

// =============================================================================
// autoClose: true with early break
// =============================================================================

async function testAutoCloseEarlyBreak() {
  const filePath = path.join(tmpDir, 'pullsync-autoclose-break.txt');
  fs.writeFileSync(filePath, 'x'.repeat(1000000));

  const fh = await open(filePath, 'r');
  // eslint-disable-next-line no-unused-vars
  for (const batch of fh.pullSync({ autoClose: true })) {
    break; // Early exit
  }

  // Handle should be closed by autoClose
  await assert.rejects(fh.stat(), { code: 'EBADF' });
}

// =============================================================================
// autoClose: false (default) - handle stays open
// =============================================================================

async function testNoAutoClose() {
  const filePath = path.join(tmpDir, 'pullsync-no-autoclose.txt');
  fs.writeFileSync(filePath, 'still open');

  const fh = await open(filePath, 'r');
  const data = textSync(fh.pullSync());
  assert.strictEqual(data, 'still open');

  // Handle should still be open and reusable
  const stat = await fh.stat();
  assert.ok(stat.size > 0);
  await fh.close();
}

// =============================================================================
// Lock semantics - pullSync locks the handle
// =============================================================================

async function testLocked() {
  const filePath = path.join(tmpDir, 'pullsync-locked.txt');
  fs.writeFileSync(filePath, 'lock test');

  const fh = await open(filePath, 'r');
  const iter = fh.pullSync()[Symbol.iterator]();
  iter.next(); // Start iteration, handle is locked

  assert.throws(() => fh.pullSync(), {
    code: 'ERR_INVALID_STATE',
  });

  assert.throws(() => fh.pull(), {
    code: 'ERR_INVALID_STATE',
  });

  // Finish iteration to unlock
  while (!iter.next().done) { /* drain */ }
  await fh.close();
}

// =============================================================================
// Empty file
// =============================================================================

async function testEmptyFile() {
  const filePath = path.join(tmpDir, 'pullsync-empty.txt');
  fs.writeFileSync(filePath, '');

  const fh = await open(filePath, 'r');
  try {
    const data = textSync(fh.pullSync());
    assert.strictEqual(data, '');
  } finally {
    await fh.close();
  }
}

// =============================================================================
// pipeToSync: file-to-file sync pipeline
// =============================================================================

async function testPipeToSync() {
  const srcPath = path.join(tmpDir, 'pullsync-pipeto-src.txt');
  const dstPath = path.join(tmpDir, 'pullsync-pipeto-dst.txt');
  const input = 'pipeToSync test data '.repeat(200);
  fs.writeFileSync(srcPath, input);

  const srcFh = await open(srcPath, 'r');
  const dstFh = await open(dstPath, 'w');
  try {
    const w = dstFh.writer();
    pipeToSync(srcFh.pullSync(), w);
  } finally {
    await srcFh.close();
    await dstFh.close();
  }

  assert.strictEqual(fs.readFileSync(dstPath, 'utf8'), input);
}

// =============================================================================
// pullSync() with start option
// =============================================================================

async function testPullSyncStart() {
  const filePath = path.join(tmpDir, 'pullsync-start.txt');
  fs.writeFileSync(filePath, 'AAABBBCCC');

  const fh = await open(filePath, 'r');
  try {
    const data = textSync(fh.pullSync({ start: 3 }));
    assert.strictEqual(data, 'BBBCCC');
  } finally {
    await fh.close();
  }
}

// =============================================================================
// pullSync() with limit option
// =============================================================================

async function testPullSyncLimit() {
  const filePath = path.join(tmpDir, 'pullsync-limit.txt');
  fs.writeFileSync(filePath, 'Hello, World! Extra data here.');

  const fh = await open(filePath, 'r');
  try {
    const data = textSync(fh.pullSync({ limit: 13 }));
    assert.strictEqual(data, 'Hello, World!');
  } finally {
    await fh.close();
  }
}

// =============================================================================
// pullSync() with start + limit
// =============================================================================

async function testPullSyncStartAndLimit() {
  const filePath = path.join(tmpDir, 'pullsync-start-limit.txt');
  fs.writeFileSync(filePath, 'AAABBBCCCDDD');

  const fh = await open(filePath, 'r');
  try {
    const data = textSync(fh.pullSync({ start: 3, limit: 3 }));
    assert.strictEqual(data, 'BBB');
  } finally {
    await fh.close();
  }
}

// =============================================================================
// pullSync() with limit spanning multiple chunks
// =============================================================================

async function testPullSyncLimitMultiChunk() {
  const filePath = path.join(tmpDir, 'pullsync-limit-multi.bin');
  const input = Buffer.alloc(300 * 1024, 'x');
  fs.writeFileSync(filePath, input);

  const fh = await open(filePath, 'r');
  try {
    const data = bytesSync(fh.pullSync({ start: 50 * 1024, limit: 200 * 1024 }));
    assert.strictEqual(data.byteLength, 200 * 1024);
  } finally {
    await fh.close();
  }
}

// =============================================================================
// pullSync() with start + limit + compression transform
// =============================================================================

async function testPullSyncStartLimitWithTransforms() {
  const filePath = path.join(tmpDir, 'pullsync-start-limit-transform.txt');
  fs.writeFileSync(filePath, 'aaabbbcccddd');

  const fh = await open(filePath, 'r');
  try {
    const compressed = fh.pullSync(compressGzipSync(),
                                   { start: 3, limit: 6 });
    const decompressed = textSync(pullSync(compressed, decompressGzipSync()));
    assert.strictEqual(decompressed, 'bbbccc');
  } finally {
    await fh.close();
  }
}

// =============================================================================
// pullSync() with start + autoClose
// =============================================================================

async function testPullSyncStartAutoClose() {
  const filePath = path.join(tmpDir, 'pullsync-start-autoclose.txt');
  fs.writeFileSync(filePath, 'AAABBBCCC');

  const fh = await open(filePath, 'r');
  const data = textSync(fh.pullSync({ start: 3, autoClose: true }));
  assert.strictEqual(data, 'BBBCCC');

  // Handle should be closed
  await assert.rejects(fh.stat(), { code: 'EBADF' });
}

// =============================================================================
// pullSync() with chunkSize option
// =============================================================================

async function testPullSyncChunkSize() {
  const filePath = path.join(tmpDir, 'pullsync-chunksize.bin');
  const input = Buffer.alloc(64 * 1024, 'z');
  fs.writeFileSync(filePath, input);

  const fh = await open(filePath, 'r');
  try {
    let batchCount = 0;
    for (const batch of fh.pullSync({ chunkSize: 16 * 1024 })) {
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

// =============================================================================
// writer() with chunkSize option (sync write threshold)
// =============================================================================

async function testWriterChunkSize() {
  const filePath = path.join(tmpDir, 'pullsync-writer-chunksize.txt');
  const fh = await open(filePath, 'w');
  // Set chunkSize to 1024 - writes larger than this should fall back to async
  const w = fh.writer({ chunkSize: 1024 });

  // Small write should succeed sync
  assert.strictEqual(w.writeSync(Buffer.alloc(512, 'a')), true);

  // Write larger than chunkSize should return false
  assert.strictEqual(w.writeSync(Buffer.alloc(2048, 'b')), false);

  await w.end();
  await fh.close();
}

// =============================================================================
// Argument validation
// =============================================================================

async function testPullArgumentValidation() {
  const filePath = path.join(tmpDir, 'pull-arg-validation.txt');
  fs.writeFileSync(filePath, 'data');

  const fh = await open(filePath, 'r');
  try {
    assert.throws(() => fh.pullSync({ autoClose: 'no' }), { code: 'ERR_INVALID_ARG_TYPE' });
    assert.throws(() => fh.pullSync({ start: 'a' }), { code: 'ERR_INVALID_ARG_TYPE' });
    assert.throws(() => fh.pullSync({ limit: 'a' }), { code: 'ERR_INVALID_ARG_TYPE' });
    assert.throws(() => fh.pullSync({ chunkSize: 'a' }), { code: 'ERR_INVALID_ARG_TYPE' });
    assert.throws(() => fh.pullSync({ start: 1.1 }), { code: 'ERR_OUT_OF_RANGE' });
    assert.throws(() => fh.pullSync({ limit: 1.1 }), { code: 'ERR_OUT_OF_RANGE' });
    assert.throws(() => fh.pullSync({ chunkSize: 1.1 }), { code: 'ERR_OUT_OF_RANGE' });
  } finally {
    await fh.close();
  }
}

// =============================================================================
// Run all tests
// =============================================================================

Promise.all([
  testBasicPullSync(),
  testLargeFile(),
  testBinaryData(),
  testPullSyncWithCompression(),
  testPullSyncWithStatelessTransform(),
  testPullSyncMixedTransforms(),
  testAutoClose(),
  testAutoCloseEarlyBreak(),
  testNoAutoClose(),
  testLocked(),
  testEmptyFile(),
  testPipeToSync(),
  testPullSyncStart(),
  testPullSyncLimit(),
  testPullSyncStartAndLimit(),
  testPullSyncLimitMultiChunk(),
  testPullSyncStartLimitWithTransforms(),
  testPullSyncStartAutoClose(),
  testPullSyncChunkSize(),
  testWriterChunkSize(),
  testPullArgumentValidation(),
]).then(common.mustCall());
