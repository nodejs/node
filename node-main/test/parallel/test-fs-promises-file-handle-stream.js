'use strict';

const common = require('../common');

// The following tests validate base functionality for the fs.promises
// FileHandle.write method.

const fs = require('fs');
const { open } = fs.promises;
const path = require('path');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const { finished } = require('stream/promises');
const { buffer } = require('stream/consumers');
const tmpDir = tmpdir.path;

tmpdir.refresh();

async function validateWrite() {
  const filePathForHandle = path.resolve(tmpDir, 'tmp-write.txt');
  const fileHandle = await open(filePathForHandle, 'w');
  const buffer = Buffer.from('Hello world'.repeat(100), 'utf8');

  const stream = fileHandle.createWriteStream();
  stream.end(buffer);
  await finished(stream);

  const readFileData = fs.readFileSync(filePathForHandle);
  assert.deepStrictEqual(buffer, readFileData);
}

async function validateRead() {
  const filePathForHandle = path.resolve(tmpDir, 'tmp-read.txt');
  const buf = Buffer.from('Hello world'.repeat(100), 'utf8');

  fs.writeFileSync(filePathForHandle, buf);

  const fileHandle = await open(filePathForHandle);
  assert.deepStrictEqual(
    await buffer(fileHandle.createReadStream()),
    buf
  );
}

Promise.all([
  validateWrite(),
  validateRead(),
]).then(common.mustCall());
