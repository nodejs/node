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
  var forkNum = 0;
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
  var listeningNum = 0;
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

  var masterExited = false;
  var workersExited = false;

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
      var alive = false;
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
