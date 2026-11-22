'use strict';

const common = require('../common');
const assert = require('node:assert/strict');
const net = require('node:net');

const message = Buffer.from('hello');
const server = net.createServer(common.mustCall((socket) => {
  server.close();
  socket.end(message);
}));

server.listen(0, '127.0.0.1', common.mustCall(() => {
  const chunks = [];
  const socket = net.connect({
    host: '127.0.0.1',
    port: server.address().port,
    onread: {
      buffer: common.mustCallAtLeast(
        () => new Uint8Array(new SharedArrayBuffer(1)), 2),
      callback: common.mustCallAtLeast((nread, buffer) => {
        chunks.push(Buffer.from(buffer.subarray(0, nread)));
      }),
    },
  });
  socket.on('data', common.mustNotCall());
  socket.on('end', common.mustCall(() => {
    assert.deepStrictEqual(Buffer.concat(chunks), message);
  }));
}));
