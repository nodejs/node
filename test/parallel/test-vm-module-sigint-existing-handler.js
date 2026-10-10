'use strict';
const common = require('../common');
if (common.isWindows) {
  // No way to send CTRL_C_EVENT to processes from JS right now.
  common.skip('platform not supported');
}

// Tests that vm.Module.prototype.evaluate({ breakOnSigint: true }) does not
// race with the process's own pre-existing SIGINT listeners, the same way
// vm.Script's runInThisContext()/runInContext() already don't (see
// test-vm-sigint-existing-handler.js).

const assert = require('assert');
const vm = require('vm');
const spawn = require('child_process').spawn;

if (process.argv[2] === 'child') {
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

  (async () => {
    const context = vm.createContext({ process });
    const mod = new vm.SourceTextModule(
      'process.send("ready"); while (true) {}',
      { context });
    await mod.link(() => {});

    await assert.rejects(
      mod.evaluate({ breakOnSigint: true }),
      { code: 'ERR_SCRIPT_EXECUTION_INTERRUPTED' },
    );
    assert.strictEqual(firstHandlerCalled, 0);
    assert.strictEqual(onceHandlerCalled, 0);

    // Keep the process alive for a while so the second SIGINT can be caught.
    const timeout = setTimeout(() => {}, 1000);

    let afterHandlerCalled = 0;
    process.on('SIGINT', common.mustCall(() => {
      // Handler attached _after_ execution.
      if (afterHandlerCalled++ === 0) {
        // The first time it just bounces back to check that the `once()`
        // handler is not called the second time.
        assert.strictEqual(firstHandlerCalled, 1);
        assert.strictEqual(onceHandlerCalled, 1);
        process.send('again');
        return;
      }

      assert.strictEqual(onceHandlerCalled, 1);
      assert.strictEqual(firstHandlerCalled, 2);
      timeout.unref();
    }, 2));

    process.send('again');
  })().then(common.mustCall());

  return;
}

const child = spawn(process.execPath, [
  '--experimental-vm-modules', __filename, 'child',
], {
  stdio: [null, 'inherit', 'inherit', 'ipc'],
});

child.on('message', common.mustCall(() => {
  // First kill() breaks the while(true) loop, second one invokes the real
  // signal handlers.
  process.kill(child.pid, 'SIGINT');
}, 3));

child.on('close', common.mustCall((code, signal) => {
  assert.strictEqual(signal, null);
  assert.strictEqual(code, 0);
}));
