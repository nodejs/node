'use strict';

const common = require('../common');
const assert = require('assert');
const net = require('net');

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

test(undefined, false, false);

const server = net.createServer(common.mustCall((socket) => {
  test(socket, true, true);
  test({ handle: socket._handle }, false, false);
  test({ handle: socket._handle, readable: true, writable: true }, true, true);
  if (socket._handle.fd >= 0) {
    test(socket._handle.fd, false, false);
    test({ fd: socket._handle.fd }, false, false);
    test({ fd: socket._handle.fd, readable: true, writable: true }, true, true);
  }

  server.close();
}));

server.listen(common.mustCall(() => {
  const { port } = server.address();
  const socket = net.connect(port, common.mustCall(() => {
    test(socket, true, true);
    socket.end();
  }));

  test(socket, false, true);
}));
