'use strict';

const common = require('../common');
const http = require('http');
const net = require('net');
const assert = require('assert');

// Verify that chunk extensions are limited in size when sent all together.
{
  const server = http.createServer((req, res) => {
    req.on('end', () => {
      res.writeHead(200, { 'Content-Type': 'text/plain' });
      res.end('bye');
    });

    req.resume();
  });

  server.listen(0, () => {
    const sock = net.connect(server.address().port);
    let data = '';

    sock.on('data', (chunk) => data += chunk.toString('utf-8'));

    sock.on('end', common.mustCall(function() {
      assert.strictEqual(data, 'HTTP/1.1 413 Payload Too Large\r\nConnection: close\r\n\r\n');
      server.close();
    }));

    sock.end('' +
      'GET / HTTP/1.1\r\n' +
      'Host: localhost:8080\r\n' +
      'Transfer-Encoding: chunked\r\n\r\n' +
      '2;' + 'A'.repeat(20000) + '=bar\r\nAA\r\n' +
      '0\r\n\r\n'
    );
  });
}

// Verify that chunk extensions are limited in size when sent in intervals.
{
  const server = http.createServer((req, res) => {
    req.on('end', () => {
      res.writeHead(200, { 'Content-Type': 'text/plain' });
      res.end('bye');
    });

    req.resume();
  });

  server.listen(0, () => {
    const sock = net.connect(server.address().port);
    let remaining = 20000;
    let data = '';

    const interval = setInterval(
      () => {
        if (remaining > 0) {
          sock.write('A'.repeat(1000));
        } else {
          sock.write('=bar\r\nAA\r\n0\r\n\r\n');
          clearInterval(interval);
        }

        remaining -= 1000;
      },
      common.platformTimeout(20),
    ).unref();

    sock.on('data', (chunk) => data += chunk.toString('utf-8'));

    sock.on('end', common.mustCall(function() {
      assert.strictEqual(data, 'HTTP/1.1 413 Payload Too Large\r\nConnection: close\r\n\r\n');
      server.close();
    }));

    sock.write('' +
    'GET / HTTP/1.1\r\n' +
    'Host: localhost:8080\r\n' +
    'Transfer-Encoding: chunked\r\n\r\n' +
    '2;'
    );
  });
}

// Verify the chunk extensions is correctly reset after a chunk
{
  const server = http.createServer((req, res) => {
    req.on('end', () => {
      res.writeHead(200, { 'content-type': 'text/plain', 'connection': 'close', 'date': 'now' });
      res.end('bye');
    });

    req.resume();
  });

  server.listen(0, () => {
    const sock = net.connect(server.address().port);
    let data = '';

    sock.on('data', (chunk) => data += chunk.toString('utf-8'));

    sock.on('end', common.mustCall(function() {
      assert.strictEqual(
        data,
        'HTTP/1.1 200 OK\r\n' +
        'content-type: text/plain\r\n' +
        'connection: close\r\n' +
        'date: now\r\n' +
        'Transfer-Encoding: chunked\r\n' +
        '\r\n' +
        '3\r\n' +
        'bye\r\n' +
        '0\r\n' +
        '\r\n',
      );

      server.close();
    }));

    sock.end('' +
      'GET / HTTP/1.1\r\n' +
      'Host: localhost:8080\r\n' +
      'Transfer-Encoding: chunked\r\n\r\n' +
      '2;' + 'A'.repeat(10000) + '=bar\r\nAA\r\n' +
      '2;' + 'A'.repeat(10000) + '=bar\r\nAA\r\n' +
      '2;' + 'A'.repeat(10000) + '=bar\r\nAA\r\n' +
      '0\r\n\r\n'
    );
  });
}
