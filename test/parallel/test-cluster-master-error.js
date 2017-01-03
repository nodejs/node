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

const totalWorkers = 2;

// Cluster setup
if (cluster.isWorker) {
  const http = require('http');
  http.Server(() => {

  }).listen(common.PORT, '127.0.0.1');

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
  }));

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

  }));

  // Startup a basic cluster
  cluster.fork();
  cluster.fork();

} else {
  // This is the testcase

  const fork = require('child_process').fork;

  let masterExited = false;
  let workersExited = false;

  // List all workers
  const workers = [];

  // Spawn a cluster process
  const master = fork(process.argv[1], ['cluster'], {silent: true});

  // Handle messages from the cluster
  master.on('message', common.mustCall((data) => {

    // Add worker pid to list and progress tracker
    if (data.cmd === 'worker') {
      workers.push(data.workerPID);
    }
  }, totalWorkers));

  // When cluster is dead
  master.on('exit', common.mustCall((code) => {

    // Check that the cluster died accidentally (non-zero exit code)
    masterExited = !!code;

    const pollWorkers = function() {
      // When master is dead all workers should be dead too
      let alive = false;
      workers.forEach((pid) => alive = common.isAlive(pid));
      if (alive) {
        setTimeout(pollWorkers, 50);
      } else {
        workersExited = true;
      }
    };

    // Loop indefinitely until worker exit
    pollWorkers();
  }));

  process.once('exit', () => {
    assert(masterExited,
           'The master did not die after an error was thrown');
    assert(workersExited,
           'The workers did not die after an error in the master');
  });

}
