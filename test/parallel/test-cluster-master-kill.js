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
  worker.once('listening', common.mustCall(() => {
    setTimeout(() => {
      process.exit(0);
    }, 1000);
  }));

} else {

  // This is the testcase
  const fork = require('child_process').fork;

  // Spawn a cluster process
  const master = fork(process.argv[1], ['cluster']);

  // get pid info
  var pid = null;
  master.once('message', (data) => {
    pid = data.pid;
  });

  // When master is dead
  var alive = true;
  master.on('exit', common.mustCall((code) => {

    // make sure that the master died on purpose
    assert.strictEqual(code, 0);

    // check worker process status
    const pollWorker = function() {
      alive = common.isAlive(pid);
      if (alive) {
        setTimeout(pollWorker, 50);
      }
    };
    // Loop indefinitely until worker exit.
    pollWorker();
  }));

  process.once('exit', () => {
    assert.strictEqual(typeof pid, 'number', 'did not get worker pid info');
    assert.strictEqual(alive, false, 'worker was alive after master died');
  });

}
