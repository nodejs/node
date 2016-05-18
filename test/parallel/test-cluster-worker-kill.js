'use strict';
// test-cluster-worker-kill.js
// verifies that, when a child process is killed (we use SIGKILL)
// - the parent receives the proper events in the proper order, no duplicates
// - the exitCode and signalCode are correct in the 'exit' event
// - the worker.exitedAfterDisconnect flag, and worker.state are correct
// - the worker process actually goes away

var common = require('../common');
var assert = require('assert');
var cluster = require('cluster');

if (cluster.isWorker) {
  var http = require('http');
  var server = http.Server(function() { });

  server.once('listening', function() { });
  server.listen(common.PORT, '127.0.0.1');

} else if (cluster.isMaster) {

  var KILL_SIGNAL = 'SIGKILL',
    expected_results = {
      cluster_emitDisconnect: [1, "the cluster did not emit 'disconnect'"],
      cluster_emitExit: [1, "the cluster did not emit 'exit'"],
      cluster_exitCode: [null, 'the cluster exited w/ incorrect exitCode'],
      cluster_signalCode: [KILL_SIGNAL,
                           'the cluster exited w/ incorrect signalCode'],
      worker_emitDisconnect: [1, "the worker did not emit 'disconnect'"],
      worker_emitExit: [1, "the worker did not emit 'exit'"],
      worker_state: ['disconnected', 'the worker state is incorrect'],
      worker_exitedAfter: [false,
                           'the .exitedAfterDisconnect flag is incorrect'],
      worker_died: [true, 'the worker is still running'],
      worker_exitCode: [null, 'the worker exited w/ incorrect exitCode'],
      worker_signalCode: [KILL_SIGNAL,
                          'the worker exited w/ incorrect signalCode']
    },
    results = {
      cluster_emitDisconnect: 0,
      cluster_emitExit: 0,
      worker_emitDisconnect: 0,
      worker_emitExit: 0
    };


  // start worker
  var worker = cluster.fork();

  // when the worker is up and running, kill it
  worker.once('listening', function() {
    worker.process.kill(KILL_SIGNAL);
  });


  // Check cluster events
  cluster.on('disconnect', function() {
    results.cluster_emitDisconnect += 1;
  });
  cluster.on('exit', function(worker) {
    results.cluster_exitCode = worker.process.exitCode;
    results.cluster_signalCode = worker.process.signalCode;
    results.cluster_emitExit += 1;
  });

  // Check worker events and properties
  worker.on('disconnect', function() {
    results.worker_emitDisconnect += 1;
    results.worker_exitedAfter = worker.exitedAfterDisconnect;
    results.worker_state = worker.state;
  });

  // Check that the worker died
  worker.once('exit', function(exitCode, signalCode) {
    results.worker_exitCode = exitCode;
    results.worker_signalCode = signalCode;
    results.worker_emitExit += 1;
    results.worker_died = !alive(worker.process.pid);
  });

  process.on('exit', function() {
    checkResults(expected_results, results);
  });
}

// some helper functions ...

function checkResults(expected_results, results) {
  for (var k in expected_results) {
    const actual = results[k];
    const expected = expected_results[k];

    var msg = (expected[1] || '') +
              (' [expected: ' + expected[0] + ' / actual: ' + actual + ']');
    if (expected && expected.length) {
      assert.equal(actual, expected[0], msg);
    } else {
      assert.equal(actual, expected, msg);
    }
  }
}

function alive(pid) {
  try {
    process.kill(pid, 'SIGCONT');
    return true;
  } catch (e) {
    return false;
  }
}
