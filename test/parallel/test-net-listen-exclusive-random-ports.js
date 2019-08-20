'use strict';

require('../common');
const assert = require('assert');
const cluster = require('cluster');
const net = require('net');

if (cluster.isMaster) {
  const worker1 = cluster.fork();

  worker1.on('message', function(port1) {
    assert.strictEqual(port1, port1 | 0,
                       `first worker could not listen on port ${port1}`);
    const worker2 = cluster.fork();

    worker2.on('message', function(port2) {
      assert.strictEqual(port2, port2 | 0,
                         `second worker could not listen on port ${port2}`);
      assert.notStrictEqual(port1, port2, 'ports should not be equal');
      worker1.kill();
      worker2.kill();
    });
  });
} else {
  const server = net.createServer(() => {});

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
