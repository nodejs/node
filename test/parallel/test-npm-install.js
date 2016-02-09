'use strict';
const common = require('../common');

const path = require('path');
const spawn = require('child_process').spawn;
const assert = require('assert');

common.refreshTmpDir();

const npmPath = path.join(
  common.testDir,
  '..',
  'deps',
  'npm',
  'bin',
  'npm-cli.js'
);

const args = [
  npmPath,
  'install'
];

const proc = spawn(process.execPath, args, {
  cwd: common.tmpDir
});

proc.on('exit', function(code) {
  // npm install in tmpDir should be essentially a no-op.
  // If npm is not broken the process should always exit 0.
  // Assert that there are no obvious failures.
  assert.equal(code, 0, 'npm install should run wihtout an error');
});
