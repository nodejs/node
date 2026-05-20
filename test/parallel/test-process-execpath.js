'use strict';
const common = require('../common');
if (common.isWindows)
  common.skip('symlinks are weird on windows');

const assert = require('assert');
const child_process = require('child_process');
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

  const proc = child_process.spawnSync(symlinkedNode, [__filename, 'child']);
  assert.strictEqual(proc.stderr.toString(), '');
  assert.strictEqual(proc.stdout.toString(), `${process.execPath}\n`);
  assert.strictEqual(proc.status, 0);
}
