var common = require('../common');
var assert = require('assert');
var cluster = require('cluster');
var net = require('net');

if (cluster.isWorker) {
  net.createServer(function(socket) {
    // Wait for any data, then close connection
    socket.on('data', socket.end.bind(socket));
  }).listen(common.PORT, common.localhostIPv4);
} else if (cluster.isMaster) {

  var connectionDone;
  var checks = {
    disconnectedOnClientsEnd: false,
    workerDied: false
  };

  // helper function to check if a process is alive
  var alive = function(pid) {
    try {
      process.kill(pid, 0);
      return true;
    } catch (e) {
      return false;
    }
  };

  // start worker
  var worker = cluster.fork();

  // Disconnect worker when it is ready
  worker.once('listening', function() {
    net.createConnection(common.PORT, common.localhostIPv4, function() {
      var socket = this;
      setTimeout(function() {
        worker.disconnect();
        setTimeout(function() {
          socket.write('.');
          connectionDone = true;
        }, 1000);
      }, 1000);
    });
  });

  // Check worker events and properties
  worker.once('disconnect', function() {
    checks.disconnectedOnClientsEnd = connectionDone;
  });

  // Check that the worker died
  worker.once('exit', function() {
    checks.workerDied = !alive(worker.process.pid);
    process.nextTick(function() {
      process.exit(0);
    });
  });

  process.once('exit', function() {
    assert.ok(checks.disconnectedOnClientsEnd, 'The worker disconnected before all clients are ended');
    assert.ok(checks.workerDied, 'The worker did not die');
  });
}
