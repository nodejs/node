'use strict';
require('../common');
var assert = require('assert');
var cluster = require('cluster');
var net = require('net');

function noop() {}

if (cluster.isMaster) {
  var worker1 = cluster.fork();

  worker1.on('message', function(obj) {
    assert.strictEqual(typeof obj, 'object');
    assert.strictEqual(obj.msg, 'success');
    var worker2 = cluster.fork({
      NODE_TEST_PORT1: obj.port1,
      NODE_TEST_PORT2: obj.port2
    });

    worker2.on('message', function(msg) {
      assert.strictEqual(msg, 'server2:EADDRINUSE');
      worker1.kill();
      worker2.kill();
    });
  });
} else {
  var server1 = net.createServer(noop);
  var server2 = net.createServer(noop);
  const port1 = (+process.env.NODE_TEST_PORT1) || 0;
  const port2 = (+process.env.NODE_TEST_PORT2) || 0;

  server1.on('error', function(err) {
    // no errors expected
    process.send('server1:' + err.code);
  });

  server2.on('error', function(err) {
    // an error is expected on the second worker
    process.send('server2:' + err.code);
  });

  server1.listen({
    host: 'localhost',
    port: port1,
    exclusive: false
  }, function() {
    server2.listen({port: port2, exclusive: true}, function() {
      // the first worker should succeed
      process.send({
        msg: 'success',
        port1: server1.address().port,
        port2: server2.address().port
      });
    });
  });
}
