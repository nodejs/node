'use strict';
const common = require('../common');
const assert = require('assert');
const child_process = require('child_process');

let patchedFork = child_process.fork;
child_process.fork = (...args) => patchedFork.apply(this, args);
const cluster = require('cluster');

if (!process.argv[2]) {
  const master = child_process.spawn(
    process.argv[0],
    [process.argv[1], '--cluster'],
    { detached: true, stdio: ['ignore', 'ignore', 'ignore', 'ipc'] });

  const messageHandlers = {
    windowsHide: common.mustCall((msg) => {
      assert.strictEqual(msg.value, true);
    }),
    workerOnline: common.mustCall((msg) => {
    }),
    mainWindowHandle: common.mustCall((msg) => {
      assert.ok(/0\s*/.test(msg.value));
    }),
    workerExit: common.mustCall((msg) => {
      assert.strictEqual(msg.code, 0);
      assert.strictEqual(msg.signal, null);
    })
  };

  master.on('message', (msg) => {
    const handler = messageHandlers[msg.type];
    assert.ok(handler);
    handler(msg);
  });

  master.on('exit', common.mustCall((code, signal) => {
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  }));

} else if (cluster.isMaster) {
  const originalFork = patchedFork;
  patchedFork = common.mustCall((...args) => {
    process.send({ type: 'windowsHide', value: args[2].windowsHide });
    return originalFork.apply(this, args);
  });

  cluster.setupMaster({
    silient: true,
    windowsHide: true
  });

  const worker = cluster.fork();
  worker.on('exit', (code, signal) => {
    process.send({ type: 'workerExit', code: code, signal: signal });
  });

  worker.on('online', (msg) => {
    process.send({ type: 'workerOnline' });

    let output = '0';
    if (process.platform === 'win32') {
      output = child_process.execSync(
        'powershell -NoProfile -c ' +
        `"(Get-Process -Id ${worker.process.pid}).MainWindowHandle"`,
        { windowsHide: true, encoding: 'utf8' });
    }

    process.send({ type: 'mainWindowHandle', value: output });
    worker.send('shutdown');
  });

} else {
  cluster.worker.on('message', (msg) => {
    cluster.worker.disconnect();
  });
}
