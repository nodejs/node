'use strict';

const common = require('../common');
const net = require('net');
const http = require('http');
const assert = require('assert');

const bodySent = 'This is my request';

function assertResponse(headers, body, expectClosed) {
  if (expectClosed) {
    assert.match(headers, /Connection: close\r\n/m);
    assert.strictEqual(headers.search(/Keep-Alive: timeout=5, max=3\r\n/m), -1);
    assert.match(body, /Hello World!/m);
  } else {
    assert.match(headers, /Connection: keep-alive\r\n/m);
    assert.match(headers, /Keep-Alive: timeout=5, max=3\r\n/m);
    assert.match(body, /Hello World!/m);
  }
}

function writeRequest(socket) {
  socket.write('POST / HTTP/1.1\r\n');
  socket.write('Host: localhost\r\n');
  socket.write('Connection: keep-alive\r\n');
  socket.write('Content-Type: text/plain\r\n');
  socket.write(`Content-Length: ${bodySent.length}\r\n\r\n`);
  socket.write(`${bodySent}\r\n`);
  socket.write('\r\n\r\n');
}

const server = http.createServer((req, res) => {
  let body = '';
  req.on('data', (data) => {
    body += data;
  });

  req.on('end', () => {
    if (req.method === 'POST') {
      assert.strictEqual(bodySent, body);
    }

    res.writeHead(200, { 'Content-Type': 'text/plain' });
    res.write('Hello World!');
    res.end();
  });
});

server.maxRequestsPerSocket = 3;

server.listen(0, common.mustCall((res) => {
  const socket = new net.Socket();

  socket.on('end', common.mustCall(() => {
    server.close();
  }));

  socket.on('ready', common.mustCall(() => {
    writeRequest(socket);
    writeRequest(socket);
    writeRequest(socket);
    writeRequest(socket);
  }));

  let buffer = '';

  socket.on('data', (data) => {
    buffer += data;

    const responseParts = buffer.trim().split('\r\n\r\n');

    if (responseParts.length === 8) {
      assertResponse(responseParts[0], responseParts[1]);
      assertResponse(responseParts[2], responseParts[3]);
      assertResponse(responseParts[4], responseParts[5], true);

      assert.match(responseParts[6], /HTTP\/1\.1 503 Service Unavailable/m);
      assert.match(responseParts[6], /Connection: close\r\n/m);
      assert.strictEqual(responseParts[6].search(/Keep-Alive: timeout=5\r\n/m), -1);
      assert.strictEqual(responseParts[7].search(/Hello World!/m), -1);

      socket.end();
    }
  });

  socket.connect({ port: server.address().port });
}));
