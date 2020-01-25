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
const fs = require('fs');
const path = require('path');

let n = 0;

// Synchronous API should return an EACCESS error with path populated.
{
  const dir = path.join(tmpdir.path, `mkdirp_${n++}`);
  fs.mkdirSync(dir);
  fs.chmodSync(dir, '444');
  let err = null;
  try {
    console.log('31: mkdirsync');
    fs.mkdirSync(path.join(dir, '/foo'), { recursive: true });
  } catch (_err) {
    console.log('34: mkdirsync', err);
    err = _err;
  }
  assert(err);
  assert.strictEqual(err.code, 'EACCES');
  assert(err.path);
}

// Asynchronous API should return an EACCESS error with path populated.
{
  const dir = path.join(tmpdir.path, `mkdirp_${n++}`);
  fs.mkdirSync(dir);
  fs.chmodSync(dir, '444');
  console.log('47: mkdir');
  fs.mkdir(path.join(dir, '/bar'), { recursive: true }, (err) => {
    console.log('49: mkdir', err);
    assert(err);
    assert.strictEqual(err.code, 'EACCES');
    assert(err.path);
  });
}
