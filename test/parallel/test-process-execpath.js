'use strict';
const common = require('../common');
const assert = require('assert');
const child_process = require('child_process');
const path = require('path');
const fs = require('fs');

assert.strictEqual(process.execPath, fs.realpathSync(process.execPath));

if (process.argv[2] === 'child') {
  console.log(process.execPath);
} else {
  common.refreshTmpDir();

  const symlinkedNode = path.join(common.tmpDir, 'symlinked-node');
  fs.symlinkSync(process.execPath, symlinkedNode);

  const proc = child_process.spawnSync(symlinkedNode, [__filename, 'child']);
  assert.strictEqual(proc.status, 0);
  assert.strictEqual(proc.stdout.toString(), `${process.execPath}\n`);
}
