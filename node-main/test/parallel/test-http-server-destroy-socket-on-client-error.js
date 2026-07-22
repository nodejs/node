'use strict';

const { expectsError, mustCall } = require('../common');

// Test that the request socket is destroyed if the `'clientError'` event is
// emitted and there is no listener for it.

const assert = require('assert');
const { createServer } = require('http');
const { createConnection } = require('net');

const server = createServer();

server.on('connection', mustCall((socket) => {
  socket.on('error', expectsError({
    name: 'Error',
    message: 'Parse Error: Invalid method encountered',
    code: 'HPE_INVALID_METHOD',
    bytesParsed: 1,
    rawPacket: Buffer.from('FOO /\r\n')
  }));
}));

server.listen(0, () => {
  const chunks = [];
  const socket = createConnection({
    allowHalfOpen: true,
    port: server.address().port
  });

  socket.on('connect', mustCall(() => {
    socket.write('FOO /\r\n');
  }));

  socket.on('data', (chunk) => {
    chunks.push(chunk);
  });

  socket.on('end', mustCall(() => {
    const expected = Buffer.from(
      'HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n'
    );
    assert(Buffer.concat(chunks).equals(expected));

    server.close();
  }));
});
