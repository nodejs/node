'use strict';
const common = require('../common');
if (common.isWindows)
  common.skip('symlinks are weird on windows');

const assert = require('assert');
const { spawnSyncAndAssert } = require('../common/child_process');
const fs = require('fs');

assert.strictEqual(process.execPath, fs.realpathSync(process.execPath));

if (process.argv[2] === 'child') {
  // The console.log() output is part of the test here.
  console.log(process.execPath);
} else {
  const tmpdir = require('../common/tmpdir');
  tmpdir.refresh();

  const symlinkedNode = tmpdir.resolve('symlinked-node');
  fs.symlinkSync(process.execPath, symlinkedNode);

  spawnSyncAndAssert(symlinkedNode, [__filename, 'child'], {
    stdout: `${process.execPath}\n`,
    stderr: ''
  });
}
