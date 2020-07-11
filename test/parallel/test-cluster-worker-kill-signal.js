// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

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
