'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const net = require('net');

if (process.argv[2] !== 'child') {
  // The parent process forks a child process, starts a TCP server, and connects
  // to the server. The accepted connection is passed to the child process,
  // where the socket is written. Then, the child signals the parent process to
  // write to the same socket.
  let result = '';

  process.on('exit', () => {
    assert.strictEqual(result, 'childparent');
  });

  const child = cp.fork(__filename, ['child']);

  // Verify that the child exits successfully
  child.on('exit', common.mustCall((exitCode, signalCode) => {
    assert.strictEqual(exitCode, 0);
    assert.strictEqual(signalCode, null);
  }));

  const server = net.createServer((socket) => {
    child.on('message', common.mustCall((msg) => {
      assert.strictEqual(msg, 'child_done');
      socket.end('parent', () => {
        server.close();
        child.disconnect();
      });
    }));

    child.send('socket', socket, { keepOpen: true }, common.mustSucceed());
  });

  server.listen(0, () => {
    const socket = net.connect(server.address().port, common.localhostIPv4);
    socket.setEncoding('utf8');
    socket.on('data', (data) => result += data);
  });
} else {
  // The child process receives the socket from the parent, writes data to
  // the socket, then signals the parent process to write
  process.on('message', common.mustCall((msg, socket) => {
    assert.strictEqual(msg, 'socket');
    socket.write('child', () => process.send('child_done'));
  }));
}
