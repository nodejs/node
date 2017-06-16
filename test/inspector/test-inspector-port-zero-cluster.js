// Flags: --inspect=0
'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const assert = require('assert');
const cluster = require('cluster');

if (cluster.isMaster) {
  const ports = [];
  for (const worker of [cluster.fork(),
                        cluster.fork(),
                        cluster.fork()]) {
    worker.on('message', (message) => {
      ports.push(message.debugPort);
      worker.kill();
    });
    worker.on('exit', (code, signal) => {
      // If the worker exited via `worker.kill()` above, exitedAfterDisconnect
      // will be true. If the worker exited because the debug port was already
      // in use, exitedAfterDisconnect will be false. That can be expected in
      // some cases, so grab the port from spawnargs.
      if (!worker.exitedAfterDisconnect) {
        const arg = worker.process.spawnargs.filter(
          (val) => /^--inspect=\d+$/.test(val)
        );
        const port = arg[0].replace('--inspect=', '');
        ports.push(+port);
      }
    });
    worker.send('debugPort');
  }
  process.on('exit', () => {
    ports.sort();
    assert.strictEqual(ports.length, 3);
    assert(ports.every((port) => port > 0));
    assert(ports.every((port) => port < 65536));
    assert.strictEqual(ports[0] + 1, ports[1]);  // Ports should be consecutive.
    assert.strictEqual(ports[1] + 1, ports[2]);
  });
} else {
  process.on('message', (message) => {
    if (message === 'debugPort')
      process.send({ debugPort: process.debugPort });
  });
}
