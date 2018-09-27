'use strict';

const common = require('../common');

// The following tests validate base functionality for the fs.promises
// FileHandle.readFile method.

const fs = require('fs');
const { open } = fs.promises;
const path = require('path');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const tmpDir = tmpdir.path;

tmpdir.refresh();

async function validateWriteFile() {
  const filePathForHandle = path.resolve(tmpDir, 'tmp-write-file2.txt');
  const fileHandle = await open(filePathForHandle, 'w+');
  const buffer = Buffer.from('Hello world'.repeat(100), 'utf8');

  await fileHandle.writeFile(buffer);
  const readFileData = fs.readFileSync(filePathForHandle);
  assert.deepStrictEqual(buffer, readFileData);
}

async function validateLargeWriteFile() {
  const fileName = path.resolve(tmpDir, 'large.txt');
  const handle = await open(fileName, 'w');
  // 16385 is written as (16384 + 1) because, 16384 is the max chunk size used
  // in the writeFile, the max chunk size is searchable, and the boundary
  // testing is also done.
  const buffer = Buffer.from(
    Array.apply(null, { length: (16384 + 1) * 3 })
      .map(Math.random)
      .map((number) => (number * (1 << 8)))
  );

  await handle.writeFile(buffer);
  await handle.close();
  assert.deepStrictEqual(fs.readFileSync(fileName), buffer);
}

validateWriteFile()
  .then(validateLargeWriteFile)
  .then(common.mustCall());
