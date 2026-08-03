'use strict';
const { mustCall } = require('../common');
const assert = require('assert');
const { createServer } = require('http');
const { createConnection } = require('net');

const server = createServer();

server.on('request', mustCall((req, res) => {
  res.write('asd', () => {
    res.socket.emit('error', new Error('kaboom'));
  });
}));

server.listen(0, mustCall(() => {
  const c = createConnection(server.address().port);
  let received = '';

  c.on('connect', mustCall(() => {
    c.write('GET /blah HTTP/1.1\r\nHost: example.com\r\n\r\n');
  }));
  c.on('data', mustCall((data) => {
    received += data.toString();
  }));
  c.on('end', mustCall(() => {
    // Should not include anything else after asd.
    assert.strictEqual(received.indexOf('asd\r\n'), received.length - 5);
    c.end();
  }));
  c.on('close', mustCall(() => server.close()));
}));
