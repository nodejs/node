'use strict';
const common = require('../common');

// This test ensures that fs.readFile correctly returns the
// contents of varying-sized files.

const tmpdir = require('../../test/common/tmpdir');
const assert = require('assert');
const fs = require('fs');

const prefix = `.removeme-fs-readfile-${process.pid}`;

tmpdir.refresh();

const fileInfo = [
  { name: tmpdir.resolve(`${prefix}-1K.txt`),
    len: 1024 },
  { name: tmpdir.resolve(`${prefix}-64K.txt`),
    len: 64 * 1024 },
  { name: tmpdir.resolve(`${prefix}-64KLessOne.txt`),
    len: (64 * 1024) - 1 },
  { name: tmpdir.resolve(`${prefix}-1M.txt`),
    len: 1 * 1024 * 1024 },
  { name: tmpdir.resolve(`${prefix}-1MPlusOne.txt`),
    len: (1 * 1024 * 1024) + 1 },
];

// Populate each fileInfo (and file) with unique fill.
const sectorSize = 512;
for (const e of fileInfo) {
  e.contents = Buffer.allocUnsafe(e.len);

  // This accounts for anything unusual in Node's implementation of readFile.
  // Using e.g. 'aa...aa' would miss bugs like Node re-reading
  // the same section twice instead of two separate sections.
  for (let offset = 0; offset < e.len; offset += sectorSize) {
    const fillByte = 256 * Math.random();
    const nBytesToFill = Math.min(sectorSize, e.len - offset);
    e.contents.fill(fillByte, offset, offset + nBytesToFill);
  }

  fs.writeFileSync(e.name, e.contents);
}
// All files are now populated.

// Test readFile on each size.
for (const e of fileInfo) {
  fs.readFile(e.name, common.mustCall((err, buf) => {
    console.log(`Validating readFile on file ${e.name} of length ${e.len}`);
    assert.ifError(err);
    assert.deepStrictEqual(buf, e.contents);
  }));
}

// readFile() and readFileSync() should fail if the file is too big.
{
  const kIoMaxLength = 2 ** 31 - 1;

  if (!tmpdir.hasEnoughSpace(kIoMaxLength)) {
    // truncateSync() will fail with ENOSPC if there is not enough space.
    common.printSkipMessage(`Not enough space in ${tmpdir.path}`);
  } else {
    const file = tmpdir.resolve(`${prefix}-too-large.txt`);
    fs.writeFileSync(file, Buffer.from('0'));
    fs.truncateSync(file, kIoMaxLength + 1);

    fs.readFile(file, common.expectsError({
      code: 'ERR_FS_FILE_TOO_LARGE',
      name: 'RangeError',
    }));
    assert.throws(() => {
      fs.readFileSync(file);
    }, { code: 'ERR_FS_FILE_TOO_LARGE', name: 'RangeError' });
  }
}

{
  // Test cancellation, before
  const signal = AbortSignal.abort();
  fs.readFile(fileInfo[0].name, { signal }, common.mustCall((err, buf) => {
    assert.strictEqual(err.name, 'AbortError');
  }));
}
{
  // Test cancellation, during read
  const controller = new AbortController();
  const signal = controller.signal;
  fs.readFile(fileInfo[0].name, { signal }, common.mustCall((err, buf) => {
    assert.strictEqual(err.name, 'AbortError');
  }));
  process.nextTick(() => controller.abort());
}
{
  // Verify that if something different than Abortcontroller.signal
  // is passed, ERR_INVALID_ARG_TYPE is thrown
  assert.throws(() => {
    const callback = common.mustNotCall();
    fs.readFile(fileInfo[0].name, { signal: 'hello' }, callback);
  }, { code: 'ERR_INVALID_ARG_TYPE', name: 'TypeError' });
}
