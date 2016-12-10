'use strict';
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');

if (cluster.isWorker) {
  const http = require('http');
  http.Server(() => {

  }).listen(common.PORT, '127.0.0.1');
  const worker = cluster.worker;
  assert.strictEqual(worker.exitedAfterDisconnect, worker.suicide);

  cluster.worker.on('disconnect', common.mustCall(() => {
    assert.strictEqual(cluster.worker.exitedAfterDisconnect,
                       cluster.worker.suicide);
    process.exit(42);
  }));

} else if (cluster.isMaster) {

  const checks = {
    cluster: {
      emitDisconnect: false,
      emitExit: false,
      callback: false
    },
    worker: {
      emitDisconnect: false,
      emitDisconnectInsideWorker: false,
      emitExit: false,
      state: false,
      voluntaryMode: false,
      died: false
    }
  };

  // start worker
  const worker = cluster.fork();

  // Disconnect worker when it is ready
  worker.once('listening', common.mustCall(() => {
    worker.disconnect();
  }));

  // Check cluster events
  cluster.once('disconnect', common.mustCall(() => {
    checks.cluster.emitDisconnect = true;
  }));
  cluster.once('exit', common.mustCall(() => {
    checks.cluster.emitExit = true;
  }));

  // Check worker events and properties
  worker.once('disconnect', common.mustCall(() => {
    checks.worker.emitDisconnect = true;
    checks.worker.voluntaryMode = worker.exitedAfterDisconnect;
    checks.worker.state = worker.state;
  }));

  // Check that the worker died
  worker.once('exit', common.mustCall((code) => {
    checks.worker.emitExit = true;
    checks.worker.died = !common.isAlive(worker.process.pid);
    checks.worker.emitDisconnectInsideWorker = code === 42;
  }));

  process.once('exit', () => {

    const w = checks.worker;
    const c = checks.cluster;

    // events
    assert.ok(w.emitDisconnect, 'Disconnect event did not emit');
    assert.ok(w.emitDisconnectInsideWorker,
              'Disconnect event did not emit inside worker');
    assert.ok(c.emitDisconnect, 'Disconnect event did not emit');
    assert.ok(w.emitExit, 'Exit event did not emit');
    assert.ok(c.emitExit, 'Exit event did not emit');

    // flags
    assert.strictEqual(w.state, 'disconnected',
                       'The state property was not set');
    assert.strictEqual(w.voluntaryMode, true,
                       'Voluntary exit mode was not set');

    // is process alive
    assert.ok(w.died, 'The worker did not die');
  });
}
