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

validateWriteFile()
  .then(common.mustCall());
