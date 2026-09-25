'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

const server = http.createServer({
  optimizeEmptyRequests: true,
}, common.mustCall((req, res) => {
  // Host / Expect lookups must not force materialization of req.headers.
  // Accessing headers afterwards still works, including unusual casing.
  assert.strictEqual(req.headers.host, 'example.test');
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.end('ok');
}, 3));

server.listen(0, common.mustCall(() => {
  const port = server.address().port;

  function request(headers) {
    return new Promise((resolve, reject) => {
      const socket = net.connect(port, '127.0.0.1', () => {
        socket.write(headers);
      });
      let data = '';
      socket.setEncoding('utf8');
      socket.on('data', (chunk) => { data += chunk; });
      socket.on('end', () => resolve(data));
      socket.on('error', reject);
    });
  }

  (async () => {
    // Mixed-case Host must satisfy requireHostHeader.
    const mixedHost = await request(
      'GET / HTTP/1.1\r\nHOST: example.test\r\nConnection: close\r\n\r\n',
    );
    assert.match(mixedHost, /^HTTP\/1\.1 200 /);

    // Unusual Host casing.
    const oddHost = await request(
      'GET / HTTP/1.1\r\nhOsT: example.test\r\nConnection: close\r\n\r\n',
    );
    assert.match(oddHost, /^HTTP\/1\.1 200 /);

    // Mixed-case Expect: 100-continue still triggers the continue path.
    const expectContinue = await request(
      'POST / HTTP/1.1\r\nHost: example.test\r\n' +
      'EXPECT: 100-continue\r\nContent-Length: 0\r\nConnection: close\r\n\r\n',
    );
    assert.match(expectContinue, /^HTTP\/1\.1 100 Continue\r\n/);
    assert.match(expectContinue, /HTTP\/1\.1 200 /);

    // Missing Host is still rejected.
    const missingHost = await request(
      'GET / HTTP/1.1\r\nConnection: close\r\n\r\n',
    );
    assert.match(missingHost, /^HTTP\/1\.1 400 /);

    server.close();
  })().then(common.mustCall());
}));
