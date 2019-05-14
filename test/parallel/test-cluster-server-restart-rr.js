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
      assert.strictEqual(
        code,
        0,
        `worker${worker2.id} did not exit normally. Exit with code: ${code}`
      );
      assert.strictEqual(
        signal,
        null,
        `worker${worker2.id} did not exit normally. Exit with signal: ${signal}`
      );
      worker1.disconnect();
    });
  }));

  worker1.on('exit', common.mustCall((code, signal) => {
    assert.strictEqual(
      code,
      0,
      `worker${worker1.id} did not exit normally. Exit with code: ${code}`
    );
    assert.strictEqual(
      signal,
      null,
      `worker${worker1.id} did not exit normally. Exit with code: ${signal}`
    );
  }));
} else {
  const net = require('net');
  const server = net.createServer();
  server.listen(0, common.mustCall(() => {
    if (cluster.worker.id === 2) {
      server.close(() => {
        server.listen(0, common.mustCall(() => {
          server.close(() => {
            process.disconnect();
          });
        }));
      });
    }
  }));
}
