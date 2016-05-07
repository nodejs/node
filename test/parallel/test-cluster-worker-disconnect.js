'use strict';
var common = require('../common');
var assert = require('assert');
var cluster = require('cluster');

if (cluster.isWorker) {
  var http = require('http');
  http.Server(function() {

  }).listen(common.PORT, '127.0.0.1');
  const worker = cluster.worker;
  assert.strictEqual(worker.exitedAfterDisconnect, worker.suicide);

  cluster.worker.on('disconnect', function() {
    assert.strictEqual(cluster.worker.exitedAfterDisconnect,
                       cluster.worker.suicide);
    process.exit(42);
  });

} else if (cluster.isMaster) {

  var checks = {
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

  // helper function to check if a process is alive
  var alive = function(pid) {
    try {
      process.kill(pid, 0);
      return true;
    } catch (e) {
      return false;
    }
  };

  // start worker
  var worker = cluster.fork();

  // Disconnect worker when it is ready
  worker.once('listening', function() {
    worker.disconnect();
  });

  // Check cluster events
  cluster.once('disconnect', function() {
    checks.cluster.emitDisconnect = true;
  });
  cluster.once('exit', function() {
    checks.cluster.emitExit = true;
  });

  // Check worker events and properties
  worker.once('disconnect', function() {
    checks.worker.emitDisconnect = true;
    checks.worker.voluntaryMode = worker.exitedAfterDisconnect;
    checks.worker.state = worker.state;
  });

  // Check that the worker died
  worker.once('exit', function(code) {
    checks.worker.emitExit = true;
    checks.worker.died = !alive(worker.process.pid);
    checks.worker.emitDisconnectInsideWorker = code === 42;
  });

  process.once('exit', function() {

    var w = checks.worker;
    var c = checks.cluster;

    // events
    assert.ok(w.emitDisconnect, 'Disconnect event did not emit');
    assert.ok(w.emitDisconnectInsideWorker,
              'Disconnect event did not emit inside worker');
    assert.ok(c.emitDisconnect, 'Disconnect event did not emit');
    assert.ok(w.emitExit, 'Exit event did not emit');
    assert.ok(c.emitExit, 'Exit event did not emit');

    // flags
    assert.equal(w.state, 'disconnected', 'The state property was not set');
    assert.equal(w.voluntaryMode, true, 'Voluntary exit mode was not set');

    // is process alive
    assert.ok(w.died, 'The worker did not die');
  });
}
