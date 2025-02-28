'use strict';

const common = require('../common');

// The following tests validate base functionality for the fs.promises
// FileHandle.stat method.

const { open } = require('fs').promises;
const tmpdir = require('../common/tmpdir');
const assert = require('assert');

tmpdir.refresh();

async function validateStat() {
  const filePath = tmpdir.resolve('tmp-read-file.txt');
  const fileHandle = await open(filePath, 'w+');
  const stats = await fileHandle.stat();
  assert.ok(stats.mtime instanceof Date);
  await fileHandle.close();
}

validateStat()
  .then(common.mustCall());
