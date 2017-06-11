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
    worker.on('message', common.mustCall((message) => {
      ports.push(message.debugPort);
      worker.kill();
    }));
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
