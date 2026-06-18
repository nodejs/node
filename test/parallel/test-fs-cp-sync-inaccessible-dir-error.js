'use strict';

// This tests that cpSync throws an error when a copied directory becomes
// inaccessible during the recursive C++ fast path.

const common = require('../common');

if (common.isIBMi) {
  common.skip('IBMi has a different access permission mechanism');
}

if (!common.isWindows && process.getuid() === 0) {
  common.skip('as this test should not be run as `root`');
}

const assert = require('assert');
const { execSync, spawnSync } = require('child_process');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

function makeDirectoryInaccessible(dir) {
  if (common.isWindows) {
    execSync(`icacls "${dir}" /deny "everyone:(OI)(CI)(DE,DC,AD,WD)"`);
  } else {
    fs.chmodSync(dir, 0o000);
  }
}

function makeDirectoryAccessible(dir) {
  if (common.isWindows) {
    execSync(`icacls "${dir}" /remove:d "everyone"`);
  } else {
    fs.chmodSync(dir, 0o755);
  }
}

if (process.argv[2] === 'child') {
  const src = process.argv[3];
  const dest = process.argv[4];
  const { cpSync } = require('fs');

  try {
    cpSync(src, dest, { recursive: true });
  } catch (err) {
    console.log(err.code || '');
    process.exit(0);
  }

  process.exit(2);
}

{
  const src = tmpdir.resolve('src');
  const dest = tmpdir.resolve('dest');
  const lockedDir = path.join(src, 'locked');

  fs.mkdirSync(lockedDir, { recursive: true });
  fs.writeFileSync(path.join(lockedDir, 'file.txt'), 'hello');

  makeDirectoryInaccessible(lockedDir);
  let result;
  try {
    result = spawnSync(process.execPath, [__filename, 'child', src, dest], {
      encoding: 'utf8',
    });
  } finally {
    makeDirectoryAccessible(lockedDir);
  }

  assert.strictEqual(result.signal, null);
  assert.strictEqual(result.status, 0);
  assert.match(result.stdout, /(EACCES|EPERM|UNKNOWN)/);
}
