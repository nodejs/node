'use strict';

const common = require('../common');

// The following tests validate base functionality for the fs.promises
// FileHandle.read method.

const fs = require('fs');
const { open } = fs.promises;
const path = require('path');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const tmpDir = tmpdir.path;

async function read(fileHandle, buffer, offset, length, position) {
  return useConf ?
    fileHandle.read({ buffer, offset, length, position }) :
    fileHandle.read(buffer, offset, length, position);
}

async function validateRead() {
  const filePath = path.resolve(tmpDir, 'tmp-read-file.txt');
  const fileHandle = await open(filePath, 'w+');
  const buffer = Buffer.from('Hello world', 'utf8');

  const fd = fs.openSync(filePath, 'w+');
  fs.writeSync(fd, buffer, 0, buffer.length);
  fs.closeSync(fd);
  const readAsyncHandle = await read(fileHandle, Buffer.alloc(11), 0, 11, 0);
  assert.deepStrictEqual(buffer.length, readAsyncHandle.bytesRead);
  assert.deepStrictEqual(buffer, readAsyncHandle.buffer);

  await fileHandle.close();
}

async function validateEmptyRead() {
  const filePath = path.resolve(tmpDir, 'tmp-read-empty-file.txt');
  const fileHandle = await open(filePath, 'w+');
  const buffer = Buffer.from('', 'utf8');

  const fd = fs.openSync(filePath, 'w+');
  fs.writeSync(fd, buffer, 0, buffer.length);
  fs.closeSync(fd);
  const readAsyncHandle = await read(fileHandle, Buffer.alloc(11), 0, 11, 0);
  assert.deepStrictEqual(buffer.length, readAsyncHandle.bytesRead);

  await fileHandle.close();
}

async function validateLargeRead() {
  // Reading beyond file length (3 in this case) should return no data.
  // This is a test for a bug where reads > uint32 would return data
  // from the current position in the file.
  const filePath = fixtures.path('x.txt');
  const fileHandle = await open(filePath, 'r');
  const pos = 0xffffffff + 1; // max-uint32 + 1
  const readHandle = await read(fileHandle, Buffer.alloc(1), 0, 1, pos);

  assert.strictEqual(readHandle.bytesRead, 0);
}

async function validateReadNoParams() {
  const filePath = fixtures.path('x.txt');
  const fileHandle = await open(filePath, 'r');
  // Should not throw
  await fileHandle.read();
}

let useConf = false;
// Validates that the zero position is respected after the position has been
// moved. The test iterates over the xyz chars twice making sure that the values
// are read from the correct position.
async function validateReadWithPositionZero() {
  const opts = { useConf: true };
  const filePath = fixtures.path('x.txt');
  const fileHandle = await open(filePath, 'r');
  const expectedSequence = ['x', 'y', 'z'];

  for (let i = 0; i < expectedSequence.length * 2; i++) {
    const len = 1;
    const pos = i % 3;
    const buf = Buffer.alloc(len);
    const { bytesRead } = await read(fileHandle, buf, 0, len, pos, opts);
    assert.strictEqual(bytesRead, len);
    assert.strictEqual(buf.toString(), expectedSequence[pos]);
  }
}

(async function() {
  for (const value of [false, true]) {
    tmpdir.refresh();
    useConf = value;

    await validateRead()
          .then(validateEmptyRead)
          .then(validateLargeRead)
          .then(common.mustCall());
  }
  await validateReadNoParams();
  await validateReadWithPositionZero();
})().then(common.mustCall());
