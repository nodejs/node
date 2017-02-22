'use strict';
const common = require('../common');
const assert = require('assert');
const vm = require('vm');

const spawn = require('child_process').spawn;

const methods = [
  'runInThisContext',
  'runInContext'
];

if (common.isWindows) {
  // No way to send CTRL_C_EVENT to processes from JS right now.
  common.skip('platform not supported');
  return;
}

if (process.argv[2] === 'child') {
  const method = process.argv[3];
  assert.ok(method);

  const script = `process.send('${method}'); while(true) {}`;
  const args = method === 'runInContext' ?
                          [vm.createContext({ process })] :
                          [];
  const options = { breakOnSigint: true };

  assert.throws(() => { vm[method](script, ...args, options); },
                /^Error: Script execution interrupted\.$/);

  return;
}

for (const method of methods) {
  const child = spawn(process.execPath, [__filename, 'child', method], {
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
