'use strict';

const common = require('../common');

// The following tests validate base functionality for the fs.promises
// FileHandle.writeFile method.

const fs = require('fs');
const { open, writeFile } = fs.promises;
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

  await fileHandle.close();
}

// Signal aborted while writing file
async function doWriteAndCancel() {
  const filePathForHandle = path.resolve(tmpDir, 'dogs-running.txt');
  const fileHandle = await open(filePathForHandle, 'w+');
  try {
    const buffer = Buffer.from('dogs running'.repeat(512 * 1024), 'utf8');
    const controller = new AbortController();
    const { signal } = controller;
    process.nextTick(() => controller.abort());
    await assert.rejects(writeFile(fileHandle, buffer, { signal }), {
      name: 'AbortError'
    });
  } finally {
    await fileHandle.close();
  }
}

validateWriteFile()
  .then(doWriteAndCancel)
  .then(common.mustCall());
