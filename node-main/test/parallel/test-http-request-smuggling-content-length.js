'use strict';

const common = require('../common');
const http = require('http');
const net = require('net');
const assert = require('assert');

// Verify that a request with a space before the content length will result
// in a 400 Bad Request.

const server = http.createServer(common.mustNotCall());

server.listen(0, common.mustCall(start));

function start() {
  const sock = net.connect(server.address().port);

  sock.write('GET / HTTP/1.1\r\nHost: localhost:5000\r\n' +
    'Content-Length : 5\r\n\r\nhello');

  let body = '';
  sock.setEncoding('utf8');
  sock.on('data', (chunk) => {
    body += chunk;
  });
  sock.on('end', common.mustCall(function() {
    assert.strictEqual(body, 'HTTP/1.1 400 Bad Request\r\n' +
      'Connection: close\r\n\r\n');
    server.close();
  }));
}
