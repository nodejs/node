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

async function validateWrite() {
  tmpdir.refresh();
  common.crashOnUnhandledRejection();

  const filePathForSync = path.resolve(tmpDir, 'tmp-write-file1.txt');
  const filePathForHandle = path.resolve(tmpDir, 'tmp-write-file2.txt');
  const fileHandle = await open(filePathForHandle, 'w+');
  const buffer = Buffer.from('Hello world'.repeat(100), 'utf8');

  const fd = fs.openSync(filePathForSync, 'w+');
  fs.writeSync(fd, buffer, 0, buffer.length);
  fs.closeSync(fd);

  const bytesRead = fs.readFileSync(filePathForSync);
  await fileHandle.write(buffer, 0, buffer.length);
  const bytesReadHandle = fs.readFileSync(filePathForHandle);
  assert.deepStrictEqual(bytesRead, bytesReadHandle);
}

async function validateEmptyWrite() {
  tmpdir.refresh();
  common.crashOnUnhandledRejection();

  const filePathForSync = path.resolve(tmpDir, 'tmp-write-file1.txt');
  const filePathForHandle = path.resolve(tmpDir, 'tmp-write-file2.txt');
  const fileHandle = await open(filePathForHandle, 'w+');
  //empty buffer
  const buffer = Buffer.from('');

  const fd = fs.openSync(filePathForSync, 'w+');
  fs.writeSync(fd, buffer, 0, buffer.length);
  fs.closeSync(fd);

  const bytesRead = fs.readFileSync(filePathForSync);
  await fileHandle.write(buffer, 0, buffer.length);
  const bytesReadHandle = fs.readFileSync(filePathForHandle);
  assert.deepStrictEqual(bytesRead, bytesReadHandle);
}

validateWrite()
  .then(validateEmptyWrite)
  .then(common.mustCall());
