'use strict';
const common = require('../common');
const assert = require('assert');
const fork = require('child_process').fork;
const net = require('net');

let isCall = false;

if (process.argv[2] === 'child') {

  process.on('message', (msg, socket) => {
    socket.end('goodbye');
  });

  process.send('hello');

} else {

  const child = fork(process.argv[1], ['child']);

  const runTest = common.mustSucceed(() => {

    const server = net.createServer();

    // server.connections should start as 0
    assert.strictEqual(server.connections, 0);
    server.getConnections(common.mustSucceed((err, conns) => {
      // `conns` should start as 0
      if (!isCall) {
        assert.ifError(err);
        assert.strictEqual(conns, 0);
        isCall = true;
      }
    }));
    server.on('connection', (socket) => {
      child.send({ what: 'socket' }, socket);
    });
    server.on('close', () => {
      child.kill();
    });

    server.listen(0, common.mustSucceed(() => {
      const connect = net.connect(server.address().port);

      connect.on('close', common.mustSucceed(() => {
        // `server.connections` should now be null.
        assert.strictEqual(server.connections, null);
        server.getConnections(common.mustSucceed((err, conns) => {
          // `conns` should now be 0.
          assert.ifError(err);
          assert.strictEqual(conns, 0);
        }));
        server.close();
      }));

      connect.resume();
    }));
  });

  child.on('message', runTest);
}
