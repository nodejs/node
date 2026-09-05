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

Promise.all([
  testAutoCloseOnEnd(),
  testAutoCloseOnFail(),
  testStartOption(),
  testStartSequentialPosition(),
  testLockedState(),
  testUnlockAfterEnd(),
  testUnlockAfterFail(),
  testWriteAfterEndRejects(),
  testClosedHandle(),
  testEndRejectsOnErrored(),
  testEndIdempotent(),
]).then(common.mustCall());
