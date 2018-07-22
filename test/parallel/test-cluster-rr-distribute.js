// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');
const net = require('net');
const { handles } = require('internal/cluster/utils');

// It should close distributed handles of new connections
// if the instance of RoundRobinHandle has been closed.
{
  if (cluster.isMaster) {
    const child = cluster.fork();

    let port = null;
    // After all workers disconnecting, the server handle will also be closed.
    // We need to store and observe it.
    let serverHandle = null;

    function createConnection() {
      const { onconnection } = serverHandle.handle;

      // Monkey-patch the `onconnection` so that we can simulate
      // some situations.
      serverHandle.handle.onconnection = (...args) => {
        // Simulate that all workers are been killed.
        child.disconnect();

        onconnection(...args);

        assert.strictEqual(
          Object.keys(handles).length,
          0,
          'The instance of RoundRobinHandle should be closed.'
        );
      };

      const socket = net.connect(port, common.mustCall());

      socket.on('error', common.mustNotCall());

      // Sockets should be closed.
      socket.on('close', common.mustCall((err) => {
        assert(!err, 'The socket should be closed without error.');
      }));
    }

    cluster.on('listening', common.mustCall(() => {
      const key = Object.keys(handles)[0];
      assert(key, 'Should create a handle.');
      serverHandle = handles[key];

      createConnection();
    }));

    cluster.on('message', common.mustCall((worker, message) => {
      port = message.port;
    }));
  } else {
    const server = net.createServer();

    server.listen(0, common.mustCall(() => {
      const { port } = server.address();
      process.send({
        port,
      });
    }));
  }
}
