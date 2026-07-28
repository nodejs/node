'use strict';
// test-cluster-worker-kill-signal.js
// verifies that when we're killing a worker using Worker.prototype.kill
// and the worker's process was killed with the given signal (SIGKILL)


const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');

if (cluster.isWorker) {
  // Make the worker run something
  const http = require('http');
  const server = http.Server(() => { });

  server.once('listening', common.mustCall());
  server.listen(0, '127.0.0.1');

} else if (cluster.isMaster) {
  const KILL_SIGNAL = 'SIGKILL';

  // Start worker
  const worker = cluster.fork();

  // When the worker is up and running, kill it
  worker.once('listening', common.mustCall(() => {
    worker.kill(KILL_SIGNAL);
  }));

  // Check worker events and properties
  worker.on('disconnect', common.mustCall(() => {
    assert.strictEqual(worker.exitedAfterDisconnect, false);
    assert.strictEqual(worker.state, 'disconnected');
  }, 1));

  // Check that the worker died
  worker.once('exit', common.mustCall((exitCode, signalCode) => {
    const isWorkerProcessStillAlive = common.isAlive(worker.process.pid);
    const numOfRunningWorkers = Object.keys(cluster.workers).length;

    assert.strictEqual(exitCode, null);
    assert.strictEqual(signalCode, KILL_SIGNAL);
    assert.strictEqual(isWorkerProcessStillAlive, false);
    assert.strictEqual(numOfRunningWorkers, 0);
  }, 1));

  // Check if the cluster was killed as well
  cluster.on('exit', common.mustCall(1));
}
