'use strict';
const common = require('../common');
const assert = require('assert');
const vm = require('vm');

const spawn = require('child_process').spawn;

if (common.isWindows) {
  // No way to send CTRL_C_EVENT to processes from JS right now.
  common.skip('platform not supported');
  return;
}

if (process.argv[2] === 'child') {
  const parent = +process.env.REPL_TEST_PPID;
  assert.ok(parent);

  let firstHandlerCalled = 0;
  process.on('SIGINT', common.mustCall(() => {
    firstHandlerCalled++;
    // Handler attached _before_ execution.
  }, 2));

  let onceHandlerCalled = 0;
  process.once('SIGINT', common.mustCall(() => {
    onceHandlerCalled++;
    // Handler attached _before_ execution.
  }));

  assert.throws(() => {
    vm.runInThisContext(`process.kill(${parent}, 'SIGUSR2'); while(true) {}`, {
      breakOnSigint: true
    });
  }, /Script execution interrupted/);

  assert.strictEqual(firstHandlerCalled, 0);
  assert.strictEqual(onceHandlerCalled, 0);

  // Keep the process alive for a while so that the second SIGINT can be caught.
  const timeout = setTimeout(() => {}, 1000);

  let afterHandlerCalled = 0;

  process.on('SIGINT', common.mustCall(() => {
    // Handler attached _after_ execution.
    if (afterHandlerCalled++ === 0) {
      // The first time it just bounces back to check that the `once()`
      // handler is not called the second time.
      process.kill(parent, 'SIGUSR2');
      return;
    }

    assert.strictEqual(onceHandlerCalled, 1);
    assert.strictEqual(firstHandlerCalled, 2);
    timeout.unref();
  }, 2));

  process.kill(parent, 'SIGUSR2');

  return;
}

process.env.REPL_TEST_PPID = process.pid;

// Set the `SIGUSR2` handler before spawning the child process to make sure
// the signal is always handled.
process.on('SIGUSR2', common.mustCall(() => {
  // First kill() breaks the while(true) loop, second one invokes the real
  // signal handlers.
  process.kill(child.pid, 'SIGINT');
}, 3));

const child = spawn(process.execPath, [__filename, 'child'], {
  stdio: [null, 'inherit', 'inherit']
});

child.on('close', function(code, signal) {
  assert.strictEqual(signal, null);
  assert.strictEqual(code, 0);
});
