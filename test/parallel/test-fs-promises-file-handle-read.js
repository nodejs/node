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
  //use async read to get the buffer that was read into
  const cb = common.mustCall(async (err, bytesRead, readBuffer) => {
    assert.ifError(err);
    fs.closeSync(fd);

    const readAsyncHandle = await fileHandle.read(Buffer.alloc(11), 0, 11, 0);
    assert.deepStrictEqual(bytesRead,
                           readAsyncHandle.bytesRead);
    assert.deepStrictEqual(readBuffer.toString('utf8'),
                           readAsyncHandle.buffer.toString('utf8'));
  });
  fs.read(fd, Buffer.alloc(11), 0, 11, 0, cb);
}

async function validateEmptyRead() {
  tmpdir.refresh();
  common.crashOnUnhandledRejection();

  const filePath = path.resolve(tmpDir, 'tmp-read-empty-file.txt');
  const fileHandle = await open(filePath, 'w+');
  const buffer = Buffer.from('', 'utf8');

  const fd = fs.openSync(filePath, 'w+');
  fs.writeSync(fd, buffer, 0, buffer.length);
  fs.closeSync(fd);
  const bytesRead = fs.readFileSync(filePath, 'utf8');
  const readAsyncHandle = await fileHandle.read(Buffer.alloc(11), 0, 11, 0);
  assert.deepStrictEqual(bytesRead.length, readAsyncHandle.bytesRead);
}

validateRead()
  .then(validateEmptyRead)
  .then(common.mustCall());
