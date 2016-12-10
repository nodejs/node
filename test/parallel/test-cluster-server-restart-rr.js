'use strict';
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');

cluster.schedulingPolicy = cluster.SCHED_RR;

if (cluster.isMaster) {
  const worker1 = cluster.fork();
  worker1.on('listening', common.mustCall(() => {
    const worker2 = cluster.fork();
    worker2.on('exit', (code, signal) => {
      assert.strictEqual(code, 0, 'worker2 did not exit normally');
      assert.strictEqual(signal, null, 'worker2 did not exit normally');
      worker1.disconnect();
    });
  }));

  worker1.on('exit', common.mustCall((code, signal) => {
    assert.strictEqual(code, 0, 'worker1 did not exit normally');
    assert.strictEqual(signal, null, 'worker1 did not exit normally');
  }));
} else {
  const net = require('net');
  const server = net.createServer();
  server.listen(common.PORT, common.mustCall(() => {
    if (cluster.worker.id === 2) {
      server.close(() => {
        server.listen(common.PORT, common.mustCall(() => {
          server.close(() => {
            process.disconnect();
          });
        }));
      });
    }
  }));
}
