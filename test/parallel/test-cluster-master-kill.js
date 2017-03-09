'use strict';
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');

if (cluster.isWorker) {

  // keep the worker alive
  const http = require('http');
  http.Server().listen(common.PORT, '127.0.0.1');

} else if (process.argv[2] === 'cluster') {

  const worker = cluster.fork();

  // send PID info to testcase process
  process.send({
    pid: worker.process.pid
  });

  // terminate the cluster process
  worker.once('listening', function() {
    setTimeout(function() {
      process.exit(0);
    }, 1000);
  });

} else {

  // This is the testcase
  const fork = require('child_process').fork;

  // is process alive helper
  const isAlive = function(pid) {
    try {
      //this will throw an error if the process is dead
      process.kill(pid, 0);

      return true;
    } catch (e) {
      return false;
    }
  };

  // Spawn a cluster process
  const master = fork(process.argv[1], ['cluster']);

  // get pid info
  let pid = null;
  master.once('message', function(data) {
    pid = data.pid;
  });

  // When master is dead
  let alive = true;
  master.on('exit', function(code) {

    // make sure that the master died on purpose
    assert.equal(code, 0);

    // check worker process status
    const pollWorker = function() {
      alive = isAlive(pid);
      if (alive) {
        setTimeout(pollWorker, 50);
      }
    };
    // Loop indefinitely until worker exit.
    pollWorker();
  });

  process.once('exit', function() {
    assert.equal(typeof pid, 'number', 'did not get worker pid info');
    assert.equal(alive, false, 'worker was alive after master died');
  });

}
