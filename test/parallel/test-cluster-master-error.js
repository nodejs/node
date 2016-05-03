'use strict';
var common = require('../common');
var assert = require('assert');
var cluster = require('cluster');

// Cluster setup
if (cluster.isWorker) {
  var http = require('http');
  http.Server(function() {

  }).listen(common.PORT, '127.0.0.1');

} else if (process.argv[2] === 'cluster') {

  var totalWorkers = 2;

  // Send PID to testcase process
  var forkNum = 0;
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
  var listeningNum = 0;
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

  var fork = require('child_process').fork;

  var isAlive = function(pid) {
    try {
      //this will throw an error if the process is dead
      process.kill(pid, 0);

      return true;
    } catch (e) {
      return false;
    }
  };

  var masterExited = false;
  var workersExited = false;

  // List all workers
  var workers = [];

  // Spawn a cluster process
  var master = fork(process.argv[1], ['cluster'], {silent: true});

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

    var pollWorkers = function() {
      // When master is dead all workers should be dead too
      var alive = false;
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
    var m = 'The master did not die after an error was thrown';
    assert.ok(masterExited, m);
    m = 'The workers did not die after an error in the master';
    assert.ok(workersExited, m);
  });

}
