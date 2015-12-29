'use strict';
// Flags: --expose-internals

const common = require('../common');
const assert = require('assert');
const net = require('net');
const cluster = require('cluster');
const handles = require('internal/cluster').handles;
const os = require('os');

if (common.isWindows) {
  console.log('1..0 # Skipped: This test does not apply to Windows.');
  return;
}

cluster.schedulingPolicy = cluster.SCHED_NONE;

if (cluster.isMaster) {
  const cpus = os.cpus().length;
  const tries = cpus > 8 ? 128 : cpus * 16;

  const worker1 = cluster.fork();
  worker1.on('message', common.mustCall(() => {
    worker1.disconnect();
    for (let i = 0; i < tries; ++ i) {
      const w = cluster.fork();
      w.on('online', common.mustCall(w.disconnect));
    }
  }));

  cluster.on('exit', common.mustCall((worker, code) => {
    assert.strictEqual(code, 0, 'worker exited with error');
  }, tries + 1));

  process.on('exit', () => {
    assert.deepEqual(Object.keys(cluster.workers), []);
    assert.strictEqual(Object.keys(handles).length, 0);
  });

  return;
}

var server = net.createServer();

server.listen(common.PORT, function() {
  process.send('listening');
});
