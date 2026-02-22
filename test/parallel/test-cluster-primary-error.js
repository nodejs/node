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
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');
const net = require('net');
const { once } = require('events');

const totalWorkers = 2;

// Cluster setup
if (cluster.isWorker) {
  const http = require('http');
  http.Server(() => {}).listen(0, '127.0.0.1');

  // Connect to the tracker server to signal liveness
  const trackerPort = process.env.TRACKER_PORT;
  if (trackerPort) {
    net.connect(trackerPort).on('error', () => {
      // Ignore errors, we just want to keep the connection open
    });
  }
} else if (process.argv[2] === 'cluster') {
  // Send PID to testcase process
  let forkNum = 0;
  cluster.on('fork', common.mustCall(function forkEvent(worker) {
    // Send PID
    process.send({
      cmd: 'worker',
      workerPID: worker.process.pid
    });

    // Stop listening when done
    if (++forkNum === totalWorkers) {
      cluster.removeListener('fork', forkEvent);
    }
  }, totalWorkers));

  // Throw accidental error when all workers are listening
  let listeningNum = 0;
  cluster.on('listening', common.mustCall(function listeningEvent() {
    // When all workers are listening
    if (++listeningNum === totalWorkers) {
      // Stop listening
      cluster.removeListener('listening', listeningEvent);

      // Throw accidental error
      process.nextTick(() => {
        throw new Error('accidental error');
      });
    }
  }, totalWorkers));

  // Startup a basic cluster
  cluster.fork();
  cluster.fork();
} else {
  // This is the testcase

  (async () => {
    const tracker = net.createServer().listen(0);
    await once(tracker, 'listening');
    const { port } = tracker.address();

    let workersAlive = 0;
    const workerDead = new Promise((resolve) => {
      tracker.on('connection', (socket) => {
        workersAlive++;
        socket.on('error', () => {});
        socket.on('close', () => {
          if (--workersAlive === 0) {
            resolve();
          }
        });
      });
    });

    const { fork } = require('child_process');

    // Spawn a cluster process
    const primary = fork(process.argv[1], ['cluster'], {
      silent: true,
      env: { ...process.env, TRACKER_PORT: port }
    });

    // Handle messages from the cluster
    primary.on('message', common.mustCall((data) => {
      // No longer need to track workers via PID for liveness
    }, totalWorkers));

    // When cluster is dead
    const [code] = await once(primary, 'exit');
    // Check that the cluster died accidentally (non-zero exit code)
    assert.strictEqual(code, 1);

    // Wait for all workers to close their connections to the tracker
    // This ensures they are actually dead without relying on PIDs.
    await workerDead;
    tracker.close();
  })().then(common.mustCall());
}
