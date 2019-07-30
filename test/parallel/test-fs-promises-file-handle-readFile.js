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

async function validateReadFile() {
  const filePath = path.resolve(tmpDir, 'tmp-read-file.txt');
  const fileHandle = await open(filePath, 'w+');
  const buffer = Buffer.from('Hello world'.repeat(100), 'utf8');

  const fd = fs.openSync(filePath, 'w+');
  fs.writeSync(fd, buffer, 0, buffer.length);
  fs.closeSync(fd);

  const readFileData = await fileHandle.readFile();
  assert.deepStrictEqual(buffer, readFileData);

  await fileHandle.close();
}

async function validateReadFileProc() {
  // Test to make sure reading a file under the /proc directory works. Adapted
  // from test-fs-read-file-sync-hostname.js.
  // Refs:
  // - https://groups.google.com/forum/#!topic/nodejs-dev/rxZ_RoH1Gn0
  // - https://github.com/nodejs/node/issues/21331

  // Test is Linux-specific.
  if (!common.isLinux)
    return;

  const fileHandle = await open('/proc/sys/kernel/hostname', 'r');
  const hostname = await fileHandle.readFile();
  assert.ok(hostname.length > 0);
}

validateReadFile()
  .then(() => validateReadFileProc())
  .then(common.mustCall());
