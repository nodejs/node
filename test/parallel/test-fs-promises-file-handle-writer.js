// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const { open } = fs.promises;
const path = require('path');
const tmpdir = require('../common/tmpdir');
const {
  pipeTo, text,
} = require('stream/iter');
const {
  compressGzip, decompressGzip,
} = require('zlib/iter');

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
// autoClose: true - handle closed after end()
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
// autoClose: true - handle closed after fail()
// =============================================================================

async function testAutoCloseOnFail() {
  const filePath = path.join(tmpDir, 'writer-autoclose-fail.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer({ autoClose: true });
  await w.write(Buffer.from('partial'));
  w.fail(new Error('test fail'));

  // Handle should be closed
  await assert.rejects(fh.stat(), { code: 'EBADF' });
  // Partial data should still be on disk (fail doesn't truncate)
  assert.strictEqual(fs.readFileSync(filePath, 'utf8'), 'partial');
}

// =============================================================================
// start option - write at specified offset
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
// start option - sequential writes advance position
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
// Locked state - can't create second writer while active
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
// Unlock after end - handle reusable
// =============================================================================

async function testUnlockAfterEnd() {
  const filePath = path.join(tmpDir, 'writer-unlock.txt');
  const fh = await open(filePath, 'w');

  const w1 = fh.writer();
  await w1.write(Buffer.from('first'));
  await w1.end();

  // Should work - handle is unlocked
  const w2 = fh.writer();
  await w2.write(Buffer.from(' second'));
  await w2.end();
  await fh.close();

  assert.strictEqual(fs.readFileSync(filePath, 'utf8'), 'first second');
}

// =============================================================================
// Unlock after fail - handle reusable
// =============================================================================

async function testUnlockAfterFail() {
  const filePath = path.join(tmpDir, 'writer-unlock-fail.txt');
  const fh = await open(filePath, 'w');

  const w1 = fh.writer();
  await w1.write(Buffer.from('failed'));
  await w1.fail(new Error('test'));

  // Should work - handle is unlocked
  const w2 = fh.writer();
  await w2.write(Buffer.from('recovered'));
  await w2.end();
  await fh.close();

  // 'recovered' is appended after 'failed' at current file offset
  const content = fs.readFileSync(filePath, 'utf8');
  assert.ok(content.startsWith('failed'));
  assert.ok(content.includes('recovered'));
}

// =============================================================================
// Write after end/fail rejects
// =============================================================================

async function testWriteAfterEndRejects() {
  const filePath = path.join(tmpDir, 'writer-closed.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer();
  await w.write(Buffer.from('data'));
  await w.end();

  await assert.rejects(w.write(Buffer.from('more')), {
    name: 'TypeError',
    message: /closed/,
  });
  await assert.rejects(w.writev([Buffer.from('more')]), {
    name: 'TypeError',
    message: /closed/,
  });

  await fh.close();
}

// =============================================================================
// Closed handle - writer() throws
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
// pipeTo() integration - pipe source through writer
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
// pipeTo() with transforms - uppercase through writer
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
// Large file write - write 1MB in 64KB chunks
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
// Symbol.asyncDispose - await using
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
  // this could fail - but it should succeed).
  const fh2 = await open(filePath, 'r');
  await fh2.close();
}

// =============================================================================
// Symbol.asyncDispose - cleanup on error (await using unwinds)
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
// Pre-aborted signal rejects write/writev/end
// =============================================================================

async function testWriteWithAbortedSignalRejects() {
  const filePath = path.join(tmpDir, 'writer-signal-write.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer();

  const ac = new AbortController();
  ac.abort();

  await assert.rejects(
    w.write(Buffer.from('data'), { signal: ac.signal }),
    { name: 'AbortError' },
  );

  // Writer should still be usable after a signal rejection
  await w.write(Buffer.from('ok'));
  await w.end();
  await fh.close();

  assert.strictEqual(fs.readFileSync(filePath, 'utf8'), 'ok');
}

async function testWritevWithAbortedSignalRejects() {
  const filePath = path.join(tmpDir, 'writer-signal-writev.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer();

  const ac = new AbortController();
  ac.abort();

  await assert.rejects(
    w.writev([Buffer.from('a'), Buffer.from('b')], { signal: ac.signal }),
    { name: 'AbortError' },
  );

  await w.writev([Buffer.from('ok')]);
  await w.end();
  await fh.close();

  assert.strictEqual(fs.readFileSync(filePath, 'utf8'), 'ok');
}

async function testEndWithAbortedSignalRejects() {
  const filePath = path.join(tmpDir, 'writer-signal-end.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer();

  await w.write(Buffer.from('data'));

  const ac = new AbortController();
  ac.abort();

  await assert.rejects(
    w.end({ signal: ac.signal }),
    { name: 'AbortError' },
  );

  // end() was rejected so writer is still open - end it cleanly
  const totalBytes = await w.end();
  await fh.close();

  assert.strictEqual(totalBytes, 4);
  assert.strictEqual(fs.readFileSync(filePath, 'utf8'), 'data');
}

// =============================================================================
// write() with string input (UTF-8 encoding)
// =============================================================================

async function testWriteString() {
  const filePath = path.join(tmpDir, 'writer-string.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer();
  await w.write('Hello ');
  await w.write('World!');
  const totalBytes = await w.end();
  await fh.close();

  assert.strictEqual(totalBytes, 12);
  assert.strictEqual(fs.readFileSync(filePath, 'utf8'), 'Hello World!');
}

// =============================================================================
// write() with string containing multi-byte UTF-8 characters
// =============================================================================

async function testWriteStringMultibyte() {
  const filePath = path.join(tmpDir, 'writer-string-multibyte.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer();
  const input = 'café ☕ 日本語';
  await w.write(input);
  const totalBytes = await w.end();
  await fh.close();

  const expected = Buffer.from(input, 'utf8');
  assert.strictEqual(totalBytes, expected.byteLength);
  assert.strictEqual(fs.readFileSync(filePath, 'utf8'), input);
}

// =============================================================================
// writev() with string chunks (UTF-8 encoding)
// =============================================================================

async function testWritevStrings() {
  const filePath = path.join(tmpDir, 'writer-writev-strings.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer();
  await w.writev(['aaa', 'bbb', 'ccc']);
  const totalBytes = await w.end();
  await fh.close();

  assert.strictEqual(totalBytes, 9);
  assert.strictEqual(fs.readFileSync(filePath, 'utf8'), 'aaabbbccc');
}

// =============================================================================
// writev() with mixed string and Uint8Array chunks
// =============================================================================

async function testWritevMixed() {
  const filePath = path.join(tmpDir, 'writer-writev-mixed.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer();
  await w.writev(['hello', Buffer.from(' '), 'world']);
  const totalBytes = await w.end();
  await fh.close();

  assert.strictEqual(totalBytes, 11);
  assert.strictEqual(fs.readFileSync(filePath, 'utf8'), 'hello world');
}

// =============================================================================
// Symbol.dispose calls fail()
// =============================================================================

async function testSyncDispose() {
  const filePath = path.join(tmpDir, 'writer-sync-dispose.txt');
  const fh = await open(filePath, 'w');

  {
    using w = fh.writer();
    await w.write(Buffer.from('before dispose'));
  }
  // Symbol.dispose calls fail(), which unlocks the handle.
  // The handle should be reusable.
  const w2 = fh.writer();
  await w2.write(Buffer.from('after dispose'));
  await w2.end();
  await fh.close();

  const content = fs.readFileSync(filePath, 'utf8');
  assert.ok(content.includes('after dispose'),
            `Expected 'after dispose' in ${JSON.stringify(content)}`);
}

// =============================================================================
// Symbol.dispose on error unwind
// =============================================================================

async function testSyncDisposeOnError() {
  const filePath = path.join(tmpDir, 'writer-sync-dispose-error.txt');
  const fh = await open(filePath, 'w');

  try {
    using w = fh.writer();
    await w.write(Buffer.from('data'));
    throw new Error('intentional');
  } catch (e) {
    assert.strictEqual(e.message, 'intentional');
  }

  // Handle should be unlocked and reusable after sync dispose
  const w2 = fh.writer();
  await w2.write(Buffer.from('recovered'));
  await w2.end();
  await fh.close();

  const content = fs.readFileSync(filePath, 'utf8');
  assert.ok(content.includes('recovered'),
            `Expected 'recovered' in ${JSON.stringify(content)}`);
}

// =============================================================================
// writeSync() basic
// =============================================================================

async function testWriteSyncBasic() {
  const filePath = path.join(tmpDir, 'writer-writesync-basic.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer();

  assert.strictEqual(w.writeSync('Hello '), true);
  assert.strictEqual(w.writeSync(Buffer.from('World!')), true);
  const totalBytes = await w.end();
  await fh.close();

  assert.strictEqual(totalBytes, 12);
  assert.strictEqual(fs.readFileSync(filePath, 'utf8'), 'Hello World!');
}

// =============================================================================
// writevSync() basic
// =============================================================================

async function testWritevSyncBasic() {
  const filePath = path.join(tmpDir, 'writer-writevsync-basic.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer();

  assert.strictEqual(w.writevSync(['aaa', Buffer.from('bbb'), 'ccc']), true);
  const totalBytes = await w.end();
  await fh.close();

  assert.strictEqual(totalBytes, 9);
  assert.strictEqual(fs.readFileSync(filePath, 'utf8'), 'aaabbbccc');
}

// =============================================================================
// writeSync() returns false for large chunks
// =============================================================================

async function testWriteSyncLargeChunk() {
  const filePath = path.join(tmpDir, 'writer-writesync-large.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer();

  // Chunk larger than 131072 should return false
  const bigChunk = Buffer.alloc(131073, 'x');
  assert.strictEqual(w.writeSync(bigChunk), false);

  // Chunk at exactly 131072 should succeed
  const exactChunk = Buffer.alloc(131072, 'y');
  assert.strictEqual(w.writeSync(exactChunk), true);

  await w.end();
  await fh.close();

  // Only the exact chunk should have been written
  const content = fs.readFileSync(filePath);
  assert.strictEqual(content.length, 131072);
}

// =============================================================================
// writeSync() returns false when async op is in flight
// =============================================================================

async function testWriteSyncReturnsFalseDuringAsync() {
  const filePath = path.join(tmpDir, 'writer-writesync-async.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer();

  // Start an async write but don't await yet
  const p = w.write(Buffer.from('async'));

  // Sync write should return false because async is in flight
  assert.strictEqual(w.writeSync(Buffer.from('sync')), false);

  await p;

  // After async completes, sync should work again
  assert.strictEqual(w.writeSync(Buffer.from(' then sync')), true);

  await w.end();
  await fh.close();

  assert.strictEqual(fs.readFileSync(filePath, 'utf8'), 'async then sync');
}

// =============================================================================
// writeSync() returns false on closed/errored writer
// =============================================================================

async function testWriteSyncClosedErrored() {
  const filePath = path.join(tmpDir, 'writer-writesync-closed.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer();

  await w.end();

  // Should return false after end()
  assert.strictEqual(w.writeSync(Buffer.from('data')), false);
  await fh.close();

  // Test errored state
  const fh2 = await open(filePath, 'w');
  const w2 = fh2.writer();
  w2.fail(new Error('test'));
  assert.strictEqual(w2.writeSync(Buffer.from('data')), false);
  await fh2.close();
}

// =============================================================================
// endSync() basic
// =============================================================================

async function testEndSyncBasic() {
  const filePath = path.join(tmpDir, 'writer-endsync-basic.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer();

  w.writeSync(Buffer.from('hello'));
  const totalBytes = w.endSync();
  await fh.close();

  assert.strictEqual(totalBytes, 5);
  assert.strictEqual(fs.readFileSync(filePath, 'utf8'), 'hello');
}

// =============================================================================
// endSync() returns -1 when async op is in flight
// =============================================================================

async function testEndSyncReturnsFalseDuringAsync() {
  const filePath = path.join(tmpDir, 'writer-endsync-async.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer();

  const p = w.write(Buffer.from('data'));
  assert.strictEqual(w.endSync(), -1);

  await p;
  const totalBytes = await w.end();
  await fh.close();

  assert.strictEqual(totalBytes, 4);
}

// =============================================================================
// endSync() idempotent on closed writer
// =============================================================================

async function testEndSyncIdempotent() {
  const filePath = path.join(tmpDir, 'writer-endsync-idempotent.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer();

  w.writeSync(Buffer.from('data'));
  const first = w.endSync();
  const second = w.endSync();

  assert.strictEqual(first, 4);
  assert.strictEqual(second, 4);  // Idempotent
  await fh.close();
}

// =============================================================================
// endSync() with autoClose fires handle.close()
// =============================================================================

async function testEndSyncAutoClose() {
  const filePath = path.join(tmpDir, 'writer-endsync-autoclose.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer({ autoClose: true });

  w.writeSync(Buffer.from('auto'));
  const totalBytes = w.endSync();

  assert.strictEqual(totalBytes, 4);

  // Handle should be closed synchronously
  await assert.rejects(fh.stat(), { code: 'EBADF' });
  assert.strictEqual(fs.readFileSync(filePath, 'utf8'), 'auto');
}

// =============================================================================
// Full sync pipeline: writeSync + endSync (no async at all)
// =============================================================================

async function testFullSyncPipeline() {
  const filePath = path.join(tmpDir, 'writer-full-sync.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer();

  // Entirely synchronous write pipeline
  w.writeSync('line 1\n');
  w.writeSync('line 2\n');
  w.writevSync(['line 3\n', 'line 4\n']);
  const totalBytes = w.endSync();
  await fh.close();

  assert.strictEqual(totalBytes, 28);
  assert.strictEqual(
    fs.readFileSync(filePath, 'utf8'),
    'line 1\nline 2\nline 3\nline 4\n',
  );
}

// =============================================================================
// end() rejects on errored writer
// =============================================================================

async function testEndRejectsOnErrored() {
  const filePath = path.join(tmpDir, 'writer-end-errored.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer();

  await w.write(Buffer.from('data'));
  w.fail(new Error('test error'));

  await assert.rejects(
    w.end(),
    { message: 'test error' },
  );
  await fh.close();
}

// =============================================================================
// end() is idempotent when closing/closed
// =============================================================================

async function testEndIdempotent() {
  const filePath = path.join(tmpDir, 'writer-end-idempotent.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer();

  await w.write(Buffer.from('data'));

  // Call end() twice concurrently - second should return same promise
  const p1 = w.end();
  const p2 = w.end();
  const [bytes1, bytes2] = await Promise.all([p1, p2]);

  assert.strictEqual(bytes1, 4);
  assert.strictEqual(bytes2, 4);

  // After closed, calling end() again returns totalBytesWritten
  const bytes3 = await w.end();
  assert.strictEqual(bytes3, 4);

  await fh.close();
}

// =============================================================================
// asyncDispose waits for pending end() when closing
// =============================================================================

async function testAsyncDisposeWhileClosing() {
  const filePath = path.join(tmpDir, 'writer-dispose-closing.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer({ autoClose: true });

  await w.write(Buffer.from('closing test'));

  // Start end() but don't await - writer is now "closing"
  const endPromise = w.end();

  // asyncDispose should wait for the pending end, not call fail()
  await w[Symbol.asyncDispose]();
  await endPromise;

  assert.strictEqual(fs.readFileSync(filePath, 'utf8'), 'closing test');
}

// =============================================================================
// asyncDispose calls fail() on open writer (not graceful cleanup)
// =============================================================================

async function testAsyncDisposeCallsFail() {
  const filePath = path.join(tmpDir, 'writer-dispose-fails.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer();

  await w.write(Buffer.from('some data'));

  // Dispose without end() - should call fail(), not graceful cleanup
  await w[Symbol.asyncDispose]();

  // Writer should be in errored state - write should reject
  await assert.rejects(
    w.write(Buffer.from('more')),
    (err) => err instanceof Error,
  );

  // Handle should be unlocked and reusable
  const w2 = fh.writer();
  await w2.end();
  await fh.close();
}

// =============================================================================
// writer() with limit - async write within limit succeeds
// =============================================================================

async function testWriterLimit() {
  const filePath = path.join(tmpDir, 'writer-limit.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer({ limit: 10 });

  await w.write(Buffer.from('12345'));  // 5 bytes, 5 remaining
  await w.write(Buffer.from('67890'));  // 5 bytes, 0 remaining
  const totalBytes = await w.end();
  await fh.close();

  assert.strictEqual(totalBytes, 10);
  assert.strictEqual(fs.readFileSync(filePath, 'utf8'), '1234567890');
}

// =============================================================================
// writer() with limit - async write exceeding limit rejects
// =============================================================================

async function testWriterLimitExceeded() {
  const filePath = path.join(tmpDir, 'writer-limit-exceeded.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer({ limit: 5 });

  await w.write(Buffer.from('123'));  // 3 bytes, 2 remaining

  await assert.rejects(
    w.write(Buffer.from('45678')),  // 5 bytes > 2 remaining
    { code: 'ERR_OUT_OF_RANGE' },
  );

  await w.end();
  await fh.close();
}

// =============================================================================
// writer() with limit - writev exceeding limit rejects
// =============================================================================

async function testWriterLimitWritev() {
  const filePath = path.join(tmpDir, 'writer-limit-writev.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer({ limit: 6 });

  await w.writev([Buffer.from('ab'), Buffer.from('cd')]);  // 4 bytes

  await assert.rejects(
    w.writev([Buffer.from('ef'), Buffer.from('gh')]),  // 4 bytes > 2 remaining
    { code: 'ERR_OUT_OF_RANGE' },
  );

  await w.end();
  await fh.close();
}

// =============================================================================
// writer() with limit - writeSync returns false when exceeding limit
// =============================================================================

async function testWriterLimitWriteSync() {
  const filePath = path.join(tmpDir, 'writer-limit-writesync.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer({ limit: 10 });

  assert.strictEqual(w.writeSync(Buffer.from('12345')), true);   // 5 ok
  assert.strictEqual(w.writeSync(Buffer.from('678')), true);     // 3 ok
  assert.strictEqual(w.writeSync(Buffer.from('901')), false);    // 3 > 2 remaining

  const totalBytes = w.endSync();
  await fh.close();

  assert.strictEqual(totalBytes, 8);
  assert.strictEqual(fs.readFileSync(filePath, 'utf8'), '12345678');
}

// =============================================================================
// writer() with limit - writevSync returns false when exceeding limit
// =============================================================================

async function testWriterLimitWritevSync() {
  const filePath = path.join(tmpDir, 'writer-limit-writevsync.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer({ limit: 5 });

  assert.strictEqual(w.writevSync([Buffer.from('ab')]), true);
  // 4 bytes > 3 remaining
  assert.strictEqual(
    w.writevSync([Buffer.from('cd'), Buffer.from('ef')]), false);

  w.endSync();
  await fh.close();
}

// =============================================================================
// writer() with limit + start
// =============================================================================

async function testWriterLimitAndStart() {
  const filePath = path.join(tmpDir, 'writer-limit-start.txt');
  // Pre-fill file with dots
  fs.writeFileSync(filePath, '...........');  // 11 dots

  const fh = await open(filePath, 'r+');
  const w = fh.writer({ start: 3, limit: 5 });

  await w.write(Buffer.from('HELLO'));  // Write at offset 3
  await w.end();
  await fh.close();

  assert.strictEqual(fs.readFileSync(filePath, 'utf8'), '...HELLO...');
}

// =============================================================================
// Argument validation
// =============================================================================

async function testWriterArgumentValidation() {
  const filePath = path.join(tmpDir, 'pull-arg-validation.txt');
  fs.writeFileSync(filePath, 'data');

  const fh = await open(filePath, 'r');
  try {
    assert.throws(() => fh.writer({ autoClose: 'no' }), { code: 'ERR_INVALID_ARG_TYPE' });
    assert.throws(() => fh.writer({ start: 'a' }), { code: 'ERR_INVALID_ARG_TYPE' });
    assert.throws(() => fh.writer({ limit: 'a' }), { code: 'ERR_INVALID_ARG_TYPE' });
    assert.throws(() => fh.writer({ chunkSize: 'a' }), { code: 'ERR_INVALID_ARG_TYPE' });
    assert.throws(() => fh.writer({ start: 1.1 }), { code: 'ERR_OUT_OF_RANGE' });
    assert.throws(() => fh.writer({ limit: 1.1 }), { code: 'ERR_OUT_OF_RANGE' });
    assert.throws(() => fh.writer({ chunkSize: 1.1 }), { code: 'ERR_OUT_OF_RANGE' });
  } finally {
    await fh.close();
  }
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
  testAutoCloseOnFail(),
  testStartOption(),
  testStartSequentialPosition(),
  testLockedState(),
  testUnlockAfterEnd(),
  testUnlockAfterFail(),
  testWriteAfterEndRejects(),
  testClosedHandle(),
  testPipeToIntegration(),
  testPipeToWithTransform(),
  testCompressRoundTrip(),
  testLargeFileWrite(),
  testAsyncDispose(),
  testAsyncDisposeOnError(),
  testWriteWithAbortedSignalRejects(),
  testWritevWithAbortedSignalRejects(),
  testEndWithAbortedSignalRejects(),
  testWriteString(),
  testWriteStringMultibyte(),
  testWritevStrings(),
  testWritevMixed(),
  testSyncDispose(),
  testSyncDisposeOnError(),
  testWriteSyncBasic(),
  testWritevSyncBasic(),
  testWriteSyncLargeChunk(),
  testWriteSyncReturnsFalseDuringAsync(),
  testWriteSyncClosedErrored(),
  testEndSyncBasic(),
  testEndSyncReturnsFalseDuringAsync(),
  testEndSyncIdempotent(),
  testEndSyncAutoClose(),
  testFullSyncPipeline(),
  testEndRejectsOnErrored(),
  testEndIdempotent(),
  testAsyncDisposeWhileClosing(),
  testAsyncDisposeCallsFail(),
  testWriterLimit(),
  testWriterLimitExceeded(),
  testWriterLimitWritev(),
  testWriterLimitWriteSync(),
  testWriterLimitWritevSync(),
  testWriterLimitAndStart(),
  testWriterArgumentValidation(),
]).then(common.mustCall());
