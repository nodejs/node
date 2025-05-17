'use strict';

const common = require('../common');

// Removing the current working directory is not supported on all platforms.
if (common.isSunOS || common.isWindows || common.isAIX || common.isIBMi) {
  common.skip('cannot rmdir current working directory on this platform');
}

const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('process.chdir is not available in Workers');
}

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();
const dirname = path.join(tmpdir.path, `cwd-deleted-test-${process.pid}`);
const originalCwd = process.cwd();

fs.mkdirSync(dirname);
process.chdir(dirname);

try {
  fs.rmdirSync(dirname);
} catch (err) {
  process.chdir(originalCwd);
  common.skip(`rmdir failed: ${err.message}. Skipping test.`);
}

assert.throws(
  () => {
    process.cwd();
  },
  {
    name: 'Error',
    message: 'Current working directory has been deleted',
  }
);

process.chdir(originalCwd);
