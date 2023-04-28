'use strict';

const common = require('../common');
const net = require('net');
const http = require('http');
const assert = require('assert');

const bodySent = 'This is my request';

function assertResponse(headers, body, expectClosed) {
  if (expectClosed) {
    assert.match(headers, /Connection: close\r\n/m);
    assert.strictEqual(headers.search(/Keep-Alive: timeout=5\r\n/m), -1);
    assert.match(body, /Hello World!/m);
  } else {
    assert.match(headers, /Connection: keep-alive\r\n/m);
    assert.match(headers, /Keep-Alive: timeout=5, max=3\r\n/m);
    assert.match(body, /Hello World!/m);
  }
}

function writeRequest(socket, withBody) {
  if (withBody) {
    socket.write('POST / HTTP/1.1\r\n');
    socket.write('Connection: keep-alive\r\n');
    socket.write('Content-Type: text/plain\r\n');
    socket.write('Host: localhost\r\n');
    socket.write(`Content-Length: ${bodySent.length}\r\n\r\n`);
    socket.write(`${bodySent}\r\n`);
    socket.write('\r\n\r\n');
  } else {
    socket.write('GET / HTTP/1.1\r\n');
    socket.write('Connection: keep-alive\r\n');
    socket.write('Host: localhost\r\n');
    socket.write('\r\n\r\n');
  }
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

function initialRequests(socket, numberOfRequests, cb) {
  let buffer = '';

  writeRequest(socket);

  socket.on('data', (data) => {
    buffer += data;

    if (buffer.endsWith('\r\n\r\n')) {
      if (--numberOfRequests === 0) {
        socket.removeAllListeners('data');
        cb();
      } else {
        const [headers, body] = buffer.trim().split('\r\n\r\n');
        assertResponse(headers, body);
        buffer = '';
        writeRequest(socket, true);
      }
    }
  });
}


server.maxRequestsPerSocket = 3;
server.listen(0, common.mustCall((res) => {
  const socket = new net.Socket();
  const anotherSocket = new net.Socket();

  socket.on('end', common.mustCall(() => {
    server.close();
  }));

  socket.on('ready', common.mustCall(() => {
    // Do 2 of 3 allowed requests and ensure they still alive
    initialRequests(socket, 2, common.mustCall(() => {
      anotherSocket.connect({ port: server.address().port });
    }));
  }));

  anotherSocket.on('ready', common.mustCall(() => {
    // Do another 2 requests with another socket
    // ensure that this will not affect the first socket
    initialRequests(anotherSocket, 2, common.mustCall(() => {
      let buffer = '';

      // Send the rest of the calls to the first socket
      // and see connection is closed
      socket.on('data', common.mustCall((data) => {
        buffer += data;

        if (buffer.endsWith('\r\n\r\n')) {
          const [headers, body] = buffer.trim().split('\r\n\r\n');
          assertResponse(headers, body, true);
          anotherSocket.end();
          socket.end();
        }
      }));

      writeRequest(socket, true);
    }));
  }));

  socket.connect({ port: server.address().port });
}));
