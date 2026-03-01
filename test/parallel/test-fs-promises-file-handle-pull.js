'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const { open } = fs.promises;
const path = require('path');
const tmpdir = require('../common/tmpdir');
const { text, bytes } = require('stream/new');

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
  // Write 64KB — enough for multiple 16KB read chunks
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
]).then(common.mustCall());
