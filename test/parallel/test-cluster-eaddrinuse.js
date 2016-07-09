'use strict';
// Check that having a worker bind to a port that's already taken doesn't
// leave the master process in a confused state. Releasing the port and
// trying again should Just Work[TM].

var common = require('../common');
var assert = require('assert');
var fork = require('child_process').fork;
var net = require('net');

var id = '' + process.argv[2];

if (id === 'undefined') {
  const server = net.createServer(common.fail);
  server.listen(common.PORT, function() {
    var worker = fork(__filename, ['worker']);
    worker.on('message', function(msg) {
      if (msg !== 'stop-listening') return;
      server.close(function() {
        worker.send('stopped-listening');
      });
    });
  });
} else if (id === 'worker') {
  let server = net.createServer(common.fail);
  server.listen(common.PORT, common.fail);
  server.on('error', common.mustCall(function(e) {
    assert(e.code, 'EADDRINUSE');
    process.send('stop-listening');
    process.once('message', function(msg) {
      if (msg !== 'stopped-listening') return;
      server = net.createServer(common.fail);
      server.listen(common.PORT, common.mustCall(function() {
        server.close();
      }));
    });
  }));
} else {
  assert(0);  // Bad argument.
}
