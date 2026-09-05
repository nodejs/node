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
  fs.writeFileSync(filePath, '...........');

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

Promise.all([
  testWriterLimit(),
  testWriterLimitExceeded(),
  testWriterLimitWritev(),
  testWriterLimitWriteSync(),
  testWriterLimitWritevSync(),
  testWriterLimitAndStart(),
  testWriterArgumentValidation(),
]).then(common.mustCall());
