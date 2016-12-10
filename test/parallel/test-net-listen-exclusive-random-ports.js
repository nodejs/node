'use strict';
require('../common');
var assert = require('assert');
var cluster = require('cluster');
var net = require('net');

function noop() {}

if (cluster.isMaster) {
  var worker1 = cluster.fork();

  worker1.on('message', function(port1) {
    assert.equal(port1, port1 | 0, 'first worker could not listen');
    var worker2 = cluster.fork();

    worker2.on('message', function(port2) {
      assert.equal(port2, port2 | 0, 'second worker could not listen');
      assert.notEqual(port1, port2, 'ports should not be equal');
      worker1.kill();
      worker2.kill();
    });
  });
} else {
  var server = net.createServer(noop);

  server.on('error', function(err) {
    process.send(err.code);
  });

  server.listen({
    port: 0,
    exclusive: true
  }, function() {
    process.send(server.address().port);
  });
}
