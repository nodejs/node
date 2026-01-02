'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');
const assert = require('assert');
const fs = require('fs');

if (!isMainThread) {
  common.skip('process.chdir is not available in Workers');
}

// Fails with EINVAL on SmartOS, EBUSY on Windows, EBUSY on AIX.
if (common.isSunOS || common.isWindows || common.isAIX || common.isIBMi) {
  common.skip('cannot rmdir current working directory');
}

const tmpdir = require('../common/tmpdir');
const dirname = `${tmpdir.path}/cwd-does-not-exist-${process.pid}`;

tmpdir.refresh();
fs.mkdirSync(dirname);
process.chdir(dirname);
fs.rmdirSync(dirname);

assert.throws(
  () => process.cwd(),
  {
    code: 'ENOENT',
    message: 'ENOENT: process.cwd failed with error no such file or directory,' +
      ' the current working directory was likely removed without changing the working directory, uv_cwd',
  }
);
