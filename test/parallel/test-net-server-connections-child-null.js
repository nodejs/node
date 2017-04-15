'use strict';
const common = require('../common');
const assert = require('assert');
const fork = require('child_process').fork;
const net = require('net');

if (process.argv[2] === 'child') {

  process.on('message', (msg, socket) => {
    socket.end('goodbye');
  });

  process.send('hello');

} else {

  const child = fork(process.argv[1], ['child']);

  const runTest = common.mustCall(() => {

    const server = net.createServer();

    // server.connections should start as 0
    assert.strictEqual(server.connections, 0);
    server.on('connection', (socket) => {
      child.send({what: 'socket'}, socket);
    });
    server.on('close', () => {
      child.kill();
    });

    server.listen(0, common.mustCall(() => {
      const connect = net.connect(server.address().port);

      connect.on('close', common.mustCall(() => {
        // now server.connections should be null
        assert.strictEqual(server.connections, null);
        server.close();
      }));
    }));
  });

  child.on('message', runTest);
}
