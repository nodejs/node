'use strict';

const common = require('../common');

if (!common.enoughTestMem)
  common.skip('intensive toString tests due to memory confinements');

const assert = require('node:assert');
const fs = require('node:fs');
const {
  open,
  truncate,
  writeFile,
} = fs.promises;
const path = require('node:path');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

async function validateReadFileLargeSparseFile() {
  const largeFileSize = 5 * 1024 * 1024 * 1024; // 5 GiB
  const filePath = path.resolve(tmpdir.path, 'file-handle-readfile-large.txt');

  if (!tmpdir.hasEnoughSpace(largeFileSize)) {
    common.printSkipMessage(`Not enough space in ${tmpdir.path}`);
    return;
  }

  await writeFile(filePath, Buffer.from('0'));
  await truncate(filePath, largeFileSize);

  await using fileHandle = await open(filePath, 'r');
  const data = await fileHandle.readFile();
  assert.strictEqual(data.length, largeFileSize);
}

validateReadFileLargeSparseFile()
  .then(common.mustCall());
