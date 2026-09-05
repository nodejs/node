'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const { open } = fs.promises;
const path = require('path');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const tmpDir = tmpdir.path;

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
  assert.ok(
    content.includes('after error'),
    `Expected 'after error' in ${JSON.stringify(content)}`,
  );
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
  assert.ok(
    content.includes('after dispose'),
    `Expected 'after dispose' in ${JSON.stringify(content)}`,
  );
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
  assert.ok(
    content.includes('recovered'),
    `Expected 'recovered' in ${JSON.stringify(content)}`,
  );
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

Promise.all([
  testAsyncDispose(),
  testAsyncDisposeOnError(),
  testSyncDispose(),
  testSyncDisposeOnError(),
  testAsyncDisposeWhileClosing(),
  testAsyncDisposeCallsFail(),
]).then(common.mustCall());
