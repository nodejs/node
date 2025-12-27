'use strict';
const common = require('../common');
// Fails with EINVAL on SmartOS, EBUSY on Windows, EBUSY on AIX.
if (common.isSunOS || common.isWindows || common.isAIX || common.isIBMi) {
  common.skip('cannot rmdir current working directory');
}

const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('process.chdir is not available in Workers');
}

const assert = require('assert');
const fs = require('fs');

const tmpdir = require('../common/tmpdir');

const dirname = `${tmpdir.path}/cwd-does-not-exist-${process.pid}`;
tmpdir.refresh();
fs.mkdirSync(dirname);
process.chdir(dirname);
fs.rmdirSync(dirname);

// Test that process.cwd() throws with an improved error message
assert.throws(
  () => process.cwd(),
  {
    code: 'ENOENT',
    message: /current working directory does not exist.*process\.cwd\(\) failed because the directory was deleted/,
  }
);
