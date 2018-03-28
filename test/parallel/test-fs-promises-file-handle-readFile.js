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

async function validateReadFile() {
  tmpdir.refresh();
  common.crashOnUnhandledRejection();

  const filePath = path.resolve(tmpDir, 'tmp-read-file.txt');
  const fileHandle = await open(filePath, 'w+');
  const buffer = Buffer.from('Hello world'.repeat(100), 'utf8');

  const fd = fs.openSync(filePath, 'w+');
  fs.writeSync(fd, buffer, 0, buffer.length);
  const readSyncData = fs.readFileSync(filePath);
  fs.closeSync(fd);

  const readAsyncData = await fileHandle.readFile();
  assert.deepStrictEqual(readSyncData, readAsyncData);
}

validateReadFile()
  .then(common.mustCall());
