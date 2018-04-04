'use strict';

const common = require('../common');

// The following tests validate base functionality for the fs/promises
// FileHandle.read method.

const fs = require('fs');
const { open } = require('fs/promises');
const path = require('path');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const tmpDir = tmpdir.path;

async function validateRead() {
  tmpdir.refresh();
  common.crashOnUnhandledRejection();

  const filePath = path.resolve(tmpDir, 'tmp-read-file.txt');
  const fileHandle = await open(filePath, 'w+');
  const buffer = Buffer.from('Hello world', 'utf8');

  const fd = fs.openSync(filePath, 'w+');
  fs.writeSync(fd, buffer, 0, buffer.length);
  fs.closeSync(fd);
  const readAsyncHandle = await fileHandle.read(Buffer.alloc(11), 0, 11, 0);
  assert.deepStrictEqual(buffer.length, readAsyncHandle.bytesRead);
  assert.deepStrictEqual(buffer, readAsyncHandle.buffer);
}

async function validateEmptyRead() {
  common.crashOnUnhandledRejection();

  const filePath = path.resolve(tmpDir, 'tmp-read-empty-file.txt');
  const fileHandle = await open(filePath, 'w+');
  const buffer = Buffer.from('', 'utf8');

  const fd = fs.openSync(filePath, 'w+');
  fs.writeSync(fd, buffer, 0, buffer.length);
  fs.closeSync(fd);
  const readAsyncHandle = await fileHandle.read(Buffer.alloc(11), 0, 11, 0);
  assert.deepStrictEqual(buffer.length, readAsyncHandle.bytesRead);
}

validateRead()
  .then(validateEmptyRead)
  .then(common.mustCall());
