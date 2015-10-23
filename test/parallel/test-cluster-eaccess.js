'use strict';
// Test that errors propagated from cluster workers are properly
// received in their master. Creates an EADDRINUSE condition by forking
// a process in child cluster and propagates the error to the master.

const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');
const fork = require('child_process').fork;
const fs = require('fs');
const net = require('net');

if (cluster.isMaster) {
  const worker = cluster.fork();

  // makes sure master is able to fork the worker
  cluster.on('fork', common.mustCall(function() {}));

  // makes sure the worker is ready
  worker.on('online', common.mustCall(function() {}));

  worker.on('message', common.mustCall(function(err) {
    // disconnect first, so that we will not leave zombies
    worker.disconnect();

    console.log(err);
    assert.strictEqual('EADDRINUSE', err.code);
  }));

  process.on('exit', function() {
    console.log('master exited');
    try {
      fs.unlinkSync(common.PIPE);
    } catch (e) {
    }
  });

} else {
  common.refreshTmpDir();
  var cp = fork(common.fixturesDir + '/listen-on-socket-and-exit.js',
                { stdio: 'inherit' });

  // message from the child indicates it's ready and listening
  cp.on('message', common.mustCall(function() {
    const server = net.createServer().listen(common.PIPE, function() {
      // message child process so that it can exit
      cp.send('end');
      // inform master about the unexpected situation
      process.send('PIPE should have been in use.');
    });

    server.on('error', function(err) {
      console.log('parent error, ending');
      // message to child process tells it to exit
      cp.send('end');
      // propagate error to parent
      process.send(err);
    });

  }));
}
