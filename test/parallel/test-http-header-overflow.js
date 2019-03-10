// Flags: --expose-internals
'use strict';
const { expectsError, mustCall } = require('../common');
const assert = require('assert');
const { createServer, maxHeaderSize } = require('http');
const { createConnection } = require('net');

const { getOptionValue } = require('internal/options');

const currentParser = getOptionValue('--http-parser');

const sumOfLengths = (strings) => strings
  .map((string) => string.length)
  .reduce((a, b) => a + b);

const getBreakingLength = () => {
  if (currentParser === 'llhttp') {
    return maxHeaderSize - HEADER_NAME.length + 1;
  } else {
    return maxHeaderSize - HEADER_NAME.length - HEADER_SEPARATOR -
      (2 * CRLF.length) + 1;
  }
};

const CRLF = '\r\n';
const HEADER_NAME = 'Cookie';
const HEADER_SEPARATOR = ': ';
const HEADER_VALUE = 'a'.repeat(getBreakingLength());
const PAYLOAD_GET = 'GET /blah HTTP/1.1';
const PAYLOAD = PAYLOAD_GET + CRLF +
  HEADER_NAME + HEADER_SEPARATOR + HEADER_VALUE + CRLF.repeat(2);

const server = createServer();

server.on('connection', mustCall((socket) => {
  socket.on('error', expectsError({
    type: Error,
    message: 'Parse Error',
    code: 'HPE_HEADER_OVERFLOW',
    bytesParsed: sumOfLengths([
      PAYLOAD_GET,
      CRLF,
      HEADER_NAME,
      HEADER_SEPARATOR,
      HEADER_VALUE
    ]) + 1,
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
      'HTTP/1.1 431 Request Header Fields Too Large\r\n' +
      'Connection: close\r\n\r\n'
    );
    c.end();
  }));
  c.on('close', mustCall(() => server.close()));
}));
