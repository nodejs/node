'use strict';
const common = require('../common');
if (common.isWindows) {
  // No way to send CTRL_C_EVENT to processes from JS right now.
  common.skip('platform not supported');
}

const assert = require('assert');
const vm = require('vm');
const spawn = require('child_process').spawn;

if (process.argv[2] === 'child') {
  const method = process.argv[3];
  const listeners = +process.argv[4];
  assert.ok(method);
  assert.ok(Number.isInteger(listeners));

  const script = `process.send('${method}'); while(true) {}`;
  const args = method === 'runInContext' ?
    [vm.createContext({ process })] :
    [];
  const options = { breakOnSigint: true };

  for (let i = 0; i < listeners; i++)
    process.on('SIGINT', common.mustNotCall());

  assert.throws(
    () => { vm[method](script, ...args, options); },
    {
      code: 'ERR_SCRIPT_EXECUTION_INTERRUPTED',
      message: 'Script execution was interrupted by `SIGINT`'
    });
  return;
}

for (const method of ['runInThisContext', 'runInContext']) {
  for (const listeners of [0, 1, 2]) {
    const args = [__filename, 'child', method, listeners];
    const child = spawn(process.execPath, args, {
      stdio: [null, 'pipe', 'inherit', 'ipc']
    });

    child.on('message', common.mustCall(() => {
      process.kill(child.pid, 'SIGINT');
    }));

    child.on('close', common.mustCall((code, signal) => {
      assert.strictEqual(signal, null);
      assert.strictEqual(code, 0);
    }));
  }
}
