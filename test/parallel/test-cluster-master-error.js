'use strict';
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');

// Cluster setup
if (cluster.isWorker) {
  const http = require('http');
  http.Server(function() {

  }).listen(common.PORT, '127.0.0.1');

} else if (process.argv[2] === 'cluster') {

  const totalWorkers = 2;

  // Send PID to testcase process
  let forkNum = 0;
  cluster.on('fork', function forkEvent(worker) {

    // Send PID
    process.send({
      cmd: 'worker',
      workerPID: worker.process.pid
    });

    // Stop listening when done
    if (++forkNum === totalWorkers) {
      cluster.removeListener('fork', forkEvent);
    }
  });

  // Throw accidental error when all workers are listening
  let listeningNum = 0;
  cluster.on('listening', function listeningEvent() {

    // When all workers are listening
    if (++listeningNum === totalWorkers) {
      // Stop listening
      cluster.removeListener('listening', listeningEvent);

      // Throw accidental error
      process.nextTick(function() {
        console.error('about to throw');
        throw new Error('accidental error');
      });
    }

  });

  // Startup a basic cluster
  cluster.fork();
  cluster.fork();

} else {
  // This is the testcase

  const fork = require('child_process').fork;

  const isAlive = function(pid) {
    try {
      //this will throw an error if the process is dead
      process.kill(pid, 0);

      return true;
    } catch (e) {
      return false;
    }
  };

  let masterExited = false;
  let workersExited = false;

  // List all workers
  const workers = [];

  // Spawn a cluster process
  const master = fork(process.argv[1], ['cluster'], {silent: true});

  // Handle messages from the cluster
  master.on('message', function(data) {

    // Add worker pid to list and progress tracker
    if (data.cmd === 'worker') {
      workers.push(data.workerPID);
    }
  });

  // When cluster is dead
  master.on('exit', function(code) {

    // Check that the cluster died accidentally (non-zero exit code)
    masterExited = !!code;

    const pollWorkers = function() {
      // When master is dead all workers should be dead too
      let alive = false;
      workers.forEach(function(pid) {
        if (isAlive(pid)) {
          alive = true;
        }
      });
      if (alive) {
        setTimeout(pollWorkers, 50);
      } else {
        workersExited = true;
      }
    };

    // Loop indefinitely until worker exit
    pollWorkers();
  });

  process.once('exit', function() {
    let m = 'The master did not die after an error was thrown';
    assert.ok(masterExited, m);
    m = 'The workers did not die after an error in the master';
    assert.ok(workersExited, m);
  });

}
