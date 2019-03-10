'use strict';
const { expectsError, mustCall } = require('../common');
const assert = require('assert');
const { createServer, maxUriSize } = require('http');
const { createConnection } = require('net');

const CRLF = '\r\n';
const REQUEST_METHOD_AND_SPACE = 'GET ';
const DUMMY_URI = '/' + 'a'.repeat(
  maxUriSize
); // The slash makes it just 1 byte too long.

const PAYLOAD = REQUEST_METHOD_AND_SPACE + DUMMY_URI + CRLF;

const server = createServer();

server.on('connection', mustCall((socket) => {
  socket.on('error', expectsError({
    type: Error,
    message: 'Parse Error',
    code: 'HPE_URI_OVERFLOW',
    bytesParsed: REQUEST_METHOD_AND_SPACE.length + DUMMY_URI.length,
    rawPacket: Buffer.from(PAYLOAD)
  }));
}));

server.listen(0, mustCall(() => {
  const c = createConnection(server.address().port);
  let received = '';

  c.on('connect', mustCall(() => {
    c.write(PAYLOAD);
  }));
  c.on('data', mustCall((data) => {
    received += data.toString();
  }));
  c.on('end', mustCall(() => {
    assert.strictEqual(
      received,
      'HTTP/1.1 414 URI Too Long\r\n' +
      'Connection: close\r\n\r\n'
    );
    c.end();
  }));
  c.on('close', mustCall(() => server.close()));
}));
