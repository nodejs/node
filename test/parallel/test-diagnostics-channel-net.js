'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');
const dc = require('diagnostics_channel');

const isNetSocket = (socket) => socket instanceof net.Socket;

dc.subscribe('net.client.socket', common.mustCall(({ socket }) => {
  assert.strictEqual(isNetSocket(socket), true);
}));

dc.subscribe('net.server.socket', common.mustCall(({ socket }) => {
  assert.strictEqual(isNetSocket(socket), true);
}));

const server = net.createServer(common.mustCall((socket) => {
  socket.destroy();
  server.close();
}));

server.listen(() => {
  const { port } = server.address();
  net.connect(port);
});
