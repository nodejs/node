'use strict';

const common = require('../common');
const assert = require('assert');
const { spawn } = require('child_process');

if (common.isWindows) {
  common.skip('Not applicable on Windows');
}

const child = spawn(process.execPath, [
  '-e',
  'require("fs").closeSync(0); setTimeout(() => {}, 2000)',
], { stdio: ['pipe', 'ignore', 'ignore'] });

const timeout = setTimeout(() => {
  assert.fail('stdin close event was not emitted');
}, 1000);

child.stdin.on('close', common.mustCall(() => {
  clearTimeout(timeout);
  child.kill();
}));

child.on('exit', common.mustCall(() => {
  clearTimeout(timeout);
}));
