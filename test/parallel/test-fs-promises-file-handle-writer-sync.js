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

  // Should return false after end()
  await w.end();
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
  // Idempotent
  assert.strictEqual(second, 4);
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

Promise.all([
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
]).then(common.mustCall());
