'use strict';
const common = require('../common');
const assert = require('assert');
const vm = require('vm');

const spawn = require('child_process').spawn;

if (process.platform === 'win32') {
  // No way to send CTRL_C_EVENT to processes from JS right now.
  common.skip('platform not supported');
  return;
}

if (process.argv[2] === 'child') {
  const parent = +process.env.REPL_TEST_PPID;
  assert.ok(parent);

  assert.throws(() => {
    vm.runInThisContext(`process.kill(${parent}, "SIGUSR2"); while(true) {}`, {
      breakOnSigint: true
    });
  }, /Script execution interrupted/);

  return;
}

process.env.REPL_TEST_PPID = process.pid;
const child = spawn(process.execPath, [ __filename, 'child' ], {
  stdio: [null, 'pipe', 'inherit']
});

process.on('SIGUSR2', common.mustCall(() => {
  process.kill(child.pid, 'SIGINT');
}));

child.on('close', function(code, signal) {
  assert.strictEqual(signal, null);
  assert.strictEqual(code, 0);
});
