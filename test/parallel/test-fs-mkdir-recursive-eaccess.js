'use strict';

// Test that mkdir with recursive option returns appropriate error
// when executed on folder it does not have permission to access.
// Ref: https://github.com/nodejs/node/issues/31481

const common = require('../common');

if (!common.isWindows && process.getuid() === 0)
  common.skip('as this test should not be run as `root`');

if (common.isIBMi)
  common.skip('IBMi has a different access permission mechanism');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const assert = require('assert');
const { execSync } = require('child_process');
const fs = require('fs');
const path = require('path');

let n = 0;

// Synchronous API should return an EACCESS error with path populated.
{
  const dir = path.join(tmpdir.path, `mkdirp_${n++}`);
  fs.mkdirSync(dir);
  let codeExpected = 'EACCES';
  if (common.isWindows) {
    codeExpected = 'EPERM';
    execSync(`icacls ${dir} /inheritance:r`);
    execSync(`icacls ${dir} /deny "everyone":W`);
  } else {
    fs.chmodSync(dir, '444');
  }
  let err = null;
  try {
    fs.mkdirSync(path.join(dir, '/foo'), { recursive: true });
  } catch (_err) {
    err = _err;
  }
  assert(err);
  assert.strictEqual(err.code, codeExpected);
  assert(err.path);
}

// Asynchronous API should return an EACCESS error with path populated.
{
  const dir = path.join(tmpdir.path, `mkdirp_${n++}`);
  fs.mkdirSync(dir);
  let codeExpected = 'EACCES';
  if (common.isWindows) {
    codeExpected = 'EPERM';
    execSync(`icacls ${dir} /inheritance:r`);
    execSync(`icacls ${dir} /deny "everyone":W`);
  } else {
    fs.chmodSync(dir, '444');
  }
  fs.mkdir(path.join(dir, '/bar'), { recursive: true }, (err) => {
    assert(err);
    assert.strictEqual(err.code, codeExpected);
    assert(err.path);
  });
}
