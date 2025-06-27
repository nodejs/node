'use strict';

const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');
const net = require('net');

assert.throws(() => {
  new net.Socket({ fd: -1 });
}, { code: 'ERR_OUT_OF_RANGE' });

assert.throws(() => {
  new net.Socket({ fd: 'foo' });
}, { code: 'ERR_INVALID_ARG_TYPE' });

function test(sock, readable, writable) {
  let socket;
  if (sock instanceof net.Socket) {
    socket = sock;
  } else {
    socket = new net.Socket(sock);
    socket.unref();
  }

  assert.strictEqual(socket.readable, readable);
  assert.strictEqual(socket.writable, writable);
}

if (cluster.isPrimary) {
  test(undefined, true, true);

  const server = net.createServer(common.mustCall((socket) => {
    socket.unref();
    test(socket, true, true);
    test({ handle: socket._handle }, true, true);
    test({ handle: socket._handle, readable: true, writable: true },
         true, true);
    server.close();
  }));

  server.listen(common.mustCall(() => {
    const { port } = server.address();
    const socket = net.connect(port, common.mustCall(() => {
      test(socket, true, true);
      socket.end();
    }));

    test(socket, true, true);
  }));

  cluster.setupPrimary({
    stdio: ['pipe', 'pipe', 'pipe', 'ipc', 'pipe', 'pipe', 'pipe']
  });

  const worker = cluster.fork();
  worker.on('exit', common.mustCall((code, signal) => {
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  }));
} else {
  test(4, true, true);
  test({ fd: 5 }, true, true);
  test({ fd: 6, readable: true, writable: true }, true, true);
  process.disconnect();
}
