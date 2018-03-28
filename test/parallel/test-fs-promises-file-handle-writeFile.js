'use strict';

const common = require('../common');

// The following tests validate base functionality for the fs/promises
// FileHandle.readFile method.

const fs = require('fs');
const { open } = require('fs/promises');
const path = require('path');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const tmpDir = tmpdir.path;

async function validateWriteFile() {
  tmpdir.refresh();
  common.crashOnUnhandledRejection();

  const filePathForSync = path.resolve(tmpDir, 'tmp-write-file1.txt');
  const filePathForHandle = path.resolve(tmpDir, 'tmp-write-file2.txt');
  const fileHandle = await open(filePathForHandle, 'w+');
  const buffer = Buffer.from('Hello world'.repeat(100), 'utf8');

  fs.writeFileSync(filePathForSync, buffer);
  const bytesRead = fs.readFileSync(filePathForSync);

  await fileHandle.writeFile(buffer);
  const bytesReadHandle = fs.readFileSync(filePathForHandle);
  assert.deepStrictEqual(bytesRead, bytesReadHandle);
}

validateWriteFile()
  .then(common.mustCall());
