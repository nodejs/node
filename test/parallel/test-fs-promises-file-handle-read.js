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

tmpdir.refresh();

async function validateRead() {
  const filePath = path.resolve(tmpDir, 'tmp-read-file.txt');
  const fileHandle = await open(filePath, 'w+');
  const buffer = Buffer.from('Hello world', 'utf8');

  const fd = fs.openSync(filePath, 'w+');
  fs.writeSync(fd, buffer, 0, buffer.length);
  fs.closeSync(fd);
  const readAsyncHandle = await fileHandle.read(Buffer.alloc(11), 0, 11, 0);
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
  const readAsyncHandle = await fileHandle.read(Buffer.alloc(11), 0, 11, 0);
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
  const readHandle = await fileHandle.read(Buffer.alloc(1), 0, 1, pos);

  assert.strictEqual(readHandle.bytesRead, 0);
}

validateRead()
  .then(validateEmptyRead)
  .then(validateLargeRead)
  .then(common.mustCall());
