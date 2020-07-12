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

  server.once('listening', common.mustCall(() => { }));
  server.listen(0, '127.0.0.1');

} else if (cluster.isMaster) {
  const KILL_SIGNAL = 'SIGKILL';
  const expectedResults = {
    emitDisconnect: {
      value: 1,
      message: "the worker did not emit 'disconnect'"
    },
    emitExit: {
      value: 1,
      message: "the worker did not emit 'exit'"
    },
    state: {
      value: 'disconnected',
      message: 'the worker state is incorrect'
    },
    exitedAfter: {
      value: false,
      message: 'the .exitedAfterDisconnect flag is incorrect'
    },
    died: {
      value: true,
      message: 'the worker is still running'
    },
    exitCode: {
      value: null,
      message: 'the worker exited w/ incorrect exitCode'
    },
    signalCode: {
      value: KILL_SIGNAL,
      message: 'the worker exited w/ incorrect signalCode'
    },
    numberOfWorkers: {
      value: 0,
      message: 'the number of workers running is wrong'
    },
  };
  const results = {
    emitDisconnect: 0,
    emitExit: 0
  };

  // Start worker
  const worker = cluster.fork();

  // When the worker is up and running, kill it
  worker.once('listening', common.mustCall(() => {
    worker.kill(KILL_SIGNAL);
  }));

  // Check worker events and properties
  worker.on('disconnect', common.mustCall(() => {
    results.emitDisconnect += 1;
    results.exitedAfter = worker.exitedAfterDisconnect;
    results.state = worker.state;
  }));

  // Check that the worker died
  worker.once('exit', common.mustCall((exitCode, signalCode) => {
    // Setting the results
    results.exitCode = exitCode;
    results.signalCode = signalCode;
    results.emitExit += 1;
    results.died = !common.isAlive(worker.process.pid);
    results.numberOfWorkers = Object.keys(cluster.workers).length;
  }));

  cluster.on('exit', common.mustCall(() => {
    // Checking if the results are as expected
    for (const [key, expected] of Object.entries(expectedResults)) {
      const actual = results[key];

      assert.strictEqual(
        actual, expected.value,
        `${expected.message} [expected: ${expected.value} / actual: ${actual}]`
      );
    }
  }));
}
