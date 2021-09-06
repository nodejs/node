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
const { Blob } = require('buffer');
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
  const buffer = Buffer.from('Hello world'.repeat(100), 'utf8');

  fs.writeFileSync(filePathForHandle, buffer);

  const fileHandle = await open(filePathForHandle);

  const chunks = [];
  for await (const chunk of fileHandle.createReadStream()) {
    chunks.push(chunk);
  }

  const arrayBuffer = await new Blob(chunks).arrayBuffer();
  assert.deepStrictEqual(Buffer.from(arrayBuffer), buffer);
}

Promise.all([
  validateWrite(),
  validateRead(),
]).then(common.mustCall());
