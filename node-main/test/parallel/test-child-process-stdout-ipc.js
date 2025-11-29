'use strict';
const common = require('../common');
const assert = require('assert');

const spawn = require('child_process').spawn;

if (process.argv[2] === 'child') {
  process.send('hahah');
  return;
}

const proc = spawn(process.execPath, [__filename, 'child'], {
  stdio: ['inherit', 'ipc', 'inherit']
});

proc.on('exit', common.mustCall(function(code) {
  assert.strictEqual(code, 0);
}));
