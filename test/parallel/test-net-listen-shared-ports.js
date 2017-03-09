'use strict';
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');
const net = require('net');

function noop() {}

if (cluster.isMaster) {
  const worker1 = cluster.fork();

  worker1.on('message', function(msg) {
    assert.equal(msg, 'success');
    const worker2 = cluster.fork();

    worker2.on('message', function(msg) {
      assert.equal(msg, 'server2:EADDRINUSE');
      worker1.kill();
      worker2.kill();
    });
  });
} else {
  const server1 = net.createServer(noop);
  const server2 = net.createServer(noop);

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
    port: common.PORT,
    exclusive: false
  }, function() {
    server2.listen({port: common.PORT + 1, exclusive: true}, function() {
      // the first worker should succeed
      process.send('success');
    });
  });
}
