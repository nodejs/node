'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const { open } = fs.promises;
const path = require('path');
const tmpdir = require('../common/tmpdir');
const {
  pipeTo, text,
  compressGzip, decompressGzip,
} = require('stream/new');

tmpdir.refresh();

const tmpDir = tmpdir.path;

// =============================================================================
// Basic write()
// =============================================================================

async function testBasicWrite() {
  const filePath = path.join(tmpDir, 'writer-basic.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer();
  await w.write(Buffer.from('Hello '));
  await w.write(Buffer.from('World!'));
  const totalBytes = await w.end();
  await fh.close();

  assert.strictEqual(totalBytes, 12);
  assert.strictEqual(fs.readFileSync(filePath, 'utf8'), 'Hello World!');
}

// =============================================================================
// Basic writev()
// =============================================================================

async function testBasicWritev() {
  const filePath = path.join(tmpDir, 'writer-writev.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer();
  await w.writev([
    Buffer.from('aaa'),
    Buffer.from('bbb'),
    Buffer.from('ccc'),
  ]);
  const totalBytes = await w.end();
  await fh.close();

  assert.strictEqual(totalBytes, 9);
  assert.strictEqual(fs.readFileSync(filePath, 'utf8'), 'aaabbbccc');
}

// =============================================================================
// Mixed write() and writev()
// =============================================================================

async function testMixedWriteAndWritev() {
  const filePath = path.join(tmpDir, 'writer-mixed.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer();
  await w.write(Buffer.from('head-'));
  await w.writev([Buffer.from('mid1-'), Buffer.from('mid2-')]);
  await w.write(Buffer.from('tail'));
  const totalBytes = await w.end();
  await fh.close();

  assert.strictEqual(totalBytes, 19);
  assert.strictEqual(fs.readFileSync(filePath, 'utf8'), 'head-mid1-mid2-tail');
}

// =============================================================================
// end() returns totalBytesWritten
// =============================================================================

async function testEndReturnsTotalBytes() {
  const filePath = path.join(tmpDir, 'writer-totalbytes.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer();

  // Write some data in various sizes
  const sizes = [100, 200, 300, 400, 500];
  let expected = 0;
  for (const size of sizes) {
    await w.write(Buffer.alloc(size, 0x41));
    expected += size;
  }
  const totalBytes = await w.end();
  await fh.close();

  assert.strictEqual(totalBytes, expected);
  assert.strictEqual(totalBytes, 1500);
  assert.strictEqual(fs.statSync(filePath).size, 1500);
}

// =============================================================================
// autoClose: true — handle closed after end()
// =============================================================================

async function testAutoCloseOnEnd() {
  const filePath = path.join(tmpDir, 'writer-autoclose-end.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer({ autoClose: true });
  await w.write(Buffer.from('auto close test'));
  await w.end();

  // Handle should be closed
  await assert.rejects(fh.stat(), { code: 'EBADF' });
  assert.strictEqual(fs.readFileSync(filePath, 'utf8'), 'auto close test');
}

// =============================================================================
// autoClose: true — handle closed after abort()
// =============================================================================

async function testAutoCloseOnAbort() {
  const filePath = path.join(tmpDir, 'writer-autoclose-abort.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer({ autoClose: true });
  await w.write(Buffer.from('partial'));
  await w.abort(new Error('test abort'));

  // Handle should be closed
  await assert.rejects(fh.stat(), { code: 'EBADF' });
  // Partial data should still be on disk (abort doesn't truncate)
  assert.strictEqual(fs.readFileSync(filePath, 'utf8'), 'partial');
}

// =============================================================================
// start option — write at specified offset
// =============================================================================

async function testStartOption() {
  const filePath = path.join(tmpDir, 'writer-start.txt');
  // Pre-fill with 10 A's
  fs.writeFileSync(filePath, 'AAAAAAAAAA');

  const fh = await open(filePath, 'r+');
  const w = fh.writer({ start: 3 });
  await w.write(Buffer.from('BBB'));
  await w.end();
  await fh.close();

  assert.strictEqual(fs.readFileSync(filePath, 'utf8'), 'AAABBBAAAA');
}

// =============================================================================
// start option — sequential writes advance position
// =============================================================================

async function testStartSequentialPosition() {
  const filePath = path.join(tmpDir, 'writer-start-seq.txt');
  fs.writeFileSync(filePath, 'XXXXXXXXXX');

  const fh = await open(filePath, 'r+');
  const w = fh.writer({ start: 2 });
  await w.write(Buffer.from('AA'));
  await w.write(Buffer.from('BB'));
  await w.writev([Buffer.from('C'), Buffer.from('D')]);
  await w.end();
  await fh.close();

  assert.strictEqual(fs.readFileSync(filePath, 'utf8'), 'XXAABBCDXX');
}

// =============================================================================
// Locked state — can't create second writer while active
// =============================================================================

async function testLockedState() {
  const filePath = path.join(tmpDir, 'writer-locked.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer();

  assert.throws(() => fh.writer(), {
    name: 'Error',
    message: /locked/,
  });

  // Also can't pull while writer is active
  assert.throws(() => fh.pull(), {
    name: 'Error',
    message: /locked/,
  });

  await w.end();
  await fh.close();
}

// =============================================================================
// Unlock after end — handle reusable
// =============================================================================

async function testUnlockAfterEnd() {
  const filePath = path.join(tmpDir, 'writer-unlock.txt');
  const fh = await open(filePath, 'w');

  const w1 = fh.writer();
  await w1.write(Buffer.from('first'));
  await w1.end();

  // Should work — handle is unlocked
  const w2 = fh.writer();
  await w2.write(Buffer.from(' second'));
  await w2.end();
  await fh.close();

  assert.strictEqual(fs.readFileSync(filePath, 'utf8'), 'first second');
}

// =============================================================================
// Unlock after abort — handle reusable
// =============================================================================

async function testUnlockAfterAbort() {
  const filePath = path.join(tmpDir, 'writer-unlock-abort.txt');
  const fh = await open(filePath, 'w');

  const w1 = fh.writer();
  await w1.write(Buffer.from('aborted'));
  await w1.abort(new Error('test'));

  // Should work — handle is unlocked
  const w2 = fh.writer();
  await w2.write(Buffer.from('recovered'));
  await w2.end();
  await fh.close();

  // 'recovered' is appended after 'aborted' at current file offset
  const content = fs.readFileSync(filePath, 'utf8');
  assert.ok(content.startsWith('aborted'));
  assert.ok(content.includes('recovered'));
}

// =============================================================================
// Write after end/abort rejects
// =============================================================================

async function testWriteAfterEndRejects() {
  const filePath = path.join(tmpDir, 'writer-closed.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer();
  await w.write(Buffer.from('data'));
  await w.end();

  await assert.rejects(w.write(Buffer.from('more')), {
    name: 'Error',
    message: /closed/,
  });
  await assert.rejects(w.writev([Buffer.from('more')]), {
    name: 'Error',
    message: /closed/,
  });

  await fh.close();
}

// =============================================================================
// Closed handle — writer() throws
// =============================================================================

async function testClosedHandle() {
  const filePath = path.join(tmpDir, 'writer-closed-handle.txt');
  const fh = await open(filePath, 'w');
  await fh.close();

  assert.throws(() => fh.writer(), {
    name: 'Error',
    message: /closed/,
  });
}

// =============================================================================
// pipeTo() integration — pipe source through writer
// =============================================================================

async function testPipeToIntegration() {
  const srcPath = path.join(tmpDir, 'writer-pipeto-src.txt');
  const dstPath = path.join(tmpDir, 'writer-pipeto-dst.txt');
  const data = 'The quick brown fox jumps over the lazy dog.\n'.repeat(500);
  fs.writeFileSync(srcPath, data);

  const rfh = await open(srcPath, 'r');
  const wfh = await open(dstPath, 'w');
  const w = wfh.writer();

  const totalBytes = await pipeTo(rfh.pull(), w);

  await rfh.close();
  await wfh.close();

  assert.strictEqual(totalBytes, Buffer.byteLength(data));
  assert.strictEqual(fs.readFileSync(dstPath, 'utf8'), data);
}

// =============================================================================
// pipeTo() with transforms — uppercase through writer
// =============================================================================

async function testPipeToWithTransform() {
  const srcPath = path.join(tmpDir, 'writer-transform-src.txt');
  const dstPath = path.join(tmpDir, 'writer-transform-dst.txt');
  const data = 'hello world from transforms test\n'.repeat(200);
  fs.writeFileSync(srcPath, data);

  function uppercase(chunks) {
    if (chunks === null) return null;
    const out = new Array(chunks.length);
    for (let i = 0; i < chunks.length; i++) {
      const src = chunks[i];
      const buf = Buffer.allocUnsafe(src.length);
      for (let j = 0; j < src.length; j++) {
        const b = src[j];
        buf[j] = (b >= 0x61 && b <= 0x7a) ? b - 0x20 : b;
      }
      out[i] = buf;
    }
    return out;
  }

  const rfh = await open(srcPath, 'r');
  const wfh = await open(dstPath, 'w');
  const w = wfh.writer();

  await pipeTo(rfh.pull(), uppercase, w);

  await rfh.close();
  await wfh.close();

  assert.strictEqual(fs.readFileSync(dstPath, 'utf8'), data.toUpperCase());
}

// =============================================================================
// Round-trip: pull → compress → writer, pull → decompress → verify
// =============================================================================

async function testCompressRoundTrip() {
  const srcPath = path.join(tmpDir, 'writer-rt-src.txt');
  const gzPath = path.join(tmpDir, 'writer-rt.gz');
  const original = 'Round trip compression test data. '.repeat(2000);
  fs.writeFileSync(srcPath, original);

  // Compress: pull → gzip → writer
  {
    const rfh = await open(srcPath, 'r');
    const wfh = await open(gzPath, 'w');
    const w = wfh.writer({ autoClose: true });
    await pipeTo(rfh.pull(), compressGzip(), w);
    await rfh.close();
  }

  // Verify compressed file is smaller
  const compressedSize = fs.statSync(gzPath).size;
  assert.ok(compressedSize < Buffer.byteLength(original),
            `Compressed ${compressedSize} should be < original ${Buffer.byteLength(original)}`);

  // Decompress: pull → gunzip → text → verify
  {
    const rfh = await open(gzPath, 'r');
    const result = await text(rfh.pull(decompressGzip()));
    await rfh.close();
    assert.strictEqual(result, original);
  }
}

// =============================================================================
// Large file write — write 1MB in 64KB chunks
// =============================================================================

async function testLargeFileWrite() {
  const filePath = path.join(tmpDir, 'writer-large.bin');
  const fh = await open(filePath, 'w');
  const w = fh.writer();

  const chunkSize = 65536;
  const totalSize = 1024 * 1024; // 1MB
  const chunk = Buffer.alloc(chunkSize, 0x42);
  let written = 0;

  while (written < totalSize) {
    await w.write(chunk);
    written += chunkSize;
  }

  const totalBytes = await w.end();
  await fh.close();

  assert.strictEqual(totalBytes, totalSize);
  assert.strictEqual(fs.statSync(filePath).size, totalSize);

  // Verify content
  const data = fs.readFileSync(filePath);
  for (let i = 0; i < data.length; i++) {
    if (data[i] !== 0x42) {
      assert.fail(`Byte at offset ${i} is ${data[i]}, expected 0x42`);
    }
  }
}

// =============================================================================
// Symbol.asyncDispose — await using
// =============================================================================

async function testAsyncDispose() {
  const filePath = path.join(tmpDir, 'writer-async-dispose.txt');
  {
    await using fh = await open(filePath, 'w');
    await using w = fh.writer({ autoClose: true });
    await w.write(Buffer.from('async dispose'));
  }
  // Both writer and file handle should be cleaned up
  assert.strictEqual(fs.readFileSync(filePath, 'utf8'), 'async dispose');

  // Verify the handle is actually closed by trying to open a new one
  // (if the old one were still open with a write lock on some OSes,
  // this could fail — but it should succeed).
  const fh2 = await open(filePath, 'r');
  await fh2.close();
}

// =============================================================================
// Symbol.asyncDispose — cleanup on error (await using unwinds)
// =============================================================================

async function testAsyncDisposeOnError() {
  const filePath = path.join(tmpDir, 'writer-dispose-error.txt');
  const fh = await open(filePath, 'w');

  try {
    await using w = fh.writer();
    await w.write(Buffer.from('before error'));
    throw new Error('intentional');
  } catch (e) {
    assert.strictEqual(e.message, 'intentional');
  }

  // If asyncDispose ran, the handle should be unlocked and reusable
  const w2 = fh.writer();
  await w2.write(Buffer.from('after error'));
  await w2.end();
  await fh.close();

  const content = fs.readFileSync(filePath, 'utf8');
  assert.ok(content.includes('after error'),
            `Expected 'after error' in ${JSON.stringify(content)}`);
}

// =============================================================================
// Run all tests
// =============================================================================

Promise.all([
  testBasicWrite(),
  testBasicWritev(),
  testMixedWriteAndWritev(),
  testEndReturnsTotalBytes(),
  testAutoCloseOnEnd(),
  testAutoCloseOnAbort(),
  testStartOption(),
  testStartSequentialPosition(),
  testLockedState(),
  testUnlockAfterEnd(),
  testUnlockAfterAbort(),
  testWriteAfterEndRejects(),
  testClosedHandle(),
  testPipeToIntegration(),
  testPipeToWithTransform(),
  testCompressRoundTrip(),
  testLargeFileWrite(),
  testAsyncDispose(),
  testAsyncDisposeOnError(),
]).then(common.mustCall());
