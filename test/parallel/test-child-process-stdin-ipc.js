'use strict';
const common = require('../common');
const assert = require('assert');

const spawn = require('child_process').spawn;

if (process.argv[2] === 'child') {
  // Just reference stdin, it should start it
  process.stdin;
  return;
}

const proc = spawn(process.execPath, [__filename, 'child'], {
  stdio: ['ipc', 'inherit', 'inherit']
});

proc.on('exit', common.mustCall(function(code) {
  assert.strictEqual(code, 0);
}));
