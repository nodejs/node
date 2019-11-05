'use strict';

const common = require('../common');
const assert = require('assert');
const child_process = require('child_process');
const path = require('path');

const releaseReg = /^v\d+\.\d+\.\d+$/;

// Npm requires crypto support.
if (!releaseReg.test(process.version) || !common.hasCrypto) {
  common.skip('This test is only for release builds');
}

{
  // Verify that npm does not print out a warning when executed.

  const npmCli = path.join(__dirname, '../../deps/npm/bin/npm-cli.js');
  const npmExec = child_process.spawnSync(process.execPath, [npmCli]);
  assert.strictEqual(npmExec.status, 1);

  const stderr = npmExec.stderr.toString();
  assert.strictEqual(stderr.length, 0, 'npm is not ready for this release ' +
                     'and is going to print warnings to users:\n' + stderr);
}
