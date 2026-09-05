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
// Pre-aborted signal rejects write/writev/end
// =============================================================================

async function testWriteWithAbortedSignalRejects() {
  const filePath = path.join(tmpDir, 'writer-signal-write.txt');
  const fh = await open(filePath, 'w');
  const w = fh.writer();

  await assert.rejects(
    w.write(Buffer.from('data'), { signal: AbortSignal.abort() }),
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

  await assert.rejects(
    w.writev([Buffer.from('a'), Buffer.from('b')], { signal: AbortSignal.abort() }),
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

  await assert.rejects(
    w.end({ signal: AbortSignal.abort() }),
    { name: 'AbortError' },
  );

  // end() was rejected so writer is still open - end it cleanly
  const totalBytes = await w.end();
  await fh.close();

  assert.strictEqual(totalBytes, 4);
  assert.strictEqual(fs.readFileSync(filePath, 'utf8'), 'data');
}

Promise.all([
  testWriteWithAbortedSignalRejects(),
  testWritevWithAbortedSignalRejects(),
  testEndWithAbortedSignalRejects(),
]).then(common.mustCall());
