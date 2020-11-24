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

async function validateRead(data, file) {
  const filePath = path.resolve(tmpDir, file);
  const buffer = Buffer.from(data, 'utf8');

  const fd = fs.openSync(filePath, 'w+');
  const fileHandle = await open(filePath, 'w+');
  const streamFileHandle = await open(filePath, 'w+');

  fs.writeSync(fd, buffer, 0, buffer.length);
  fs.closeSync(fd);

  fileHandle.on('close', common.mustCall());
  const readAsyncHandle = await read(fileHandle, Buffer.alloc(11), 0, 11, 0);
  assert.deepStrictEqual(data.length, readAsyncHandle.bytesRead);
  if (data.length)
    assert.deepStrictEqual(buffer, readAsyncHandle.buffer);
  await fileHandle.close();

  const stream = fs.createReadStream(null, { fd: streamFileHandle });
  let streamData = Buffer.alloc(0);
  for await (const chunk of stream)
    streamData = Buffer.from(chunk);
  assert.deepStrictEqual(buffer, streamData);
  if (data.length)
    assert.deepStrictEqual(streamData, readAsyncHandle.buffer);
  await streamFileHandle.close();
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

let useConf = false;

(async function() {
  for (const value of [false, true]) {
    tmpdir.refresh();
    useConf = value;

    await validateRead('Hello world', 'tmp-read-file.txt')
      .then(validateRead('', 'tmp-read-empty-file.txt'))
      .then(validateLargeRead)
      .then(common.mustCall());
  }
})().then(common.mustCall());
