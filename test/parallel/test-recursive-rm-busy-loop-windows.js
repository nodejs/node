'use strict';
const common = require('../common');

// This test ensures that recursive rm throws EACCES instead of
// going into a busy-loop for this scenario:
// https://github.com/nodejs/node/issues/34580

const assert = require('assert');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');
const { join } = require('path');

if (common.isIBMi)
  common.skip('IBMi has a different access permission mechanism');

if (!(common.isWindows || process.getuid() !== 0))
  common.skip('Test is not supposed to be run as root.');

tmpdir.refresh();

function makeDirectoryReadOnly(dir) {
  if (common.isWindows) {
    execSync(`icacls ${dir} /deny "everyone:(OI)(CI)(DE,DC)"`);
  } else {
    fs.chmodSync(dir, 0o444);
  }
}

function makeDirectoryWritable(dir) {
  if (common.isWindows) {
    execSync(`icacls ${dir} /remove:d "everyone"`);
  } else {
    fs.chmodSync(dirname, 0o777);
  }
}

function rmdirRecursiveSync() {
  const root = fs.mkdtempSync(tmpdir.path);

  const middle = join(root, 'middle');
  fs.mkdirSync(middle);
  fs.mkdirSync(join(middle, 'leaf')); // Make `middle` non-empty
  makeDirectoryReadOnly(middle);

  try {
    assert.throws(() => {
      fs.rmSync(root, { recursive: true });
    }, /EACCES/);
  } finally {
    makeDirectoryWritable(middle);
  }
}

rmdirRecursiveSync();
