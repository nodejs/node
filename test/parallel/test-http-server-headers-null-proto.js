'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

// Test 1: req.headers has a null prototype
{
  const server = http.createServer(common.mustCall((req, res) => {
    assert.strictEqual(Object.getPrototypeOf(req.headers), null);
    res.end();
  }));

  server.listen(0, common.mustCall(() => {
    const port = server.address().port;

    const client = net.connect(port, common.mustCall(() => {
      client.write(
        'GET / HTTP/1.1\r\n' +
        'Host: localhost\r\n' +
        'Connection: close\r\n' +
        '\r\n',
      );
    }));

    client.on('end', common.mustCall(() => {
      server.close();
    }));

    client.resume();
  }));
}

// Test 2: req.trailers has a null prototype
{
  const server = http.createServer(common.mustCall((req, res) => {
    req.on('end', common.mustCall(() => {
      assert.strictEqual(Object.getPrototypeOf(req.trailers), null);
      assert.strictEqual(req.trailers['x-client-trailer'], 'bar');
    }));
    res.setHeader('Transfer-Encoding', 'chunked');
    res.setHeader('Trailer', 'X-Trailer');
    res.write('Hello');
    res.addTrailers({
      'X-Trailer': 'test',
    });
    res.end();
  }));

  server.listen(0, common.mustCall(() => {
    const port = server.address().port;

    const client = net.connect(port, common.mustCall(() => {
      client.write(
        'GET / HTTP/1.1\r\n' +
        'Host: localhost\r\n' +
        'Transfer-Encoding: chunked\r\n' +
        'Trailer: X-Client-Trailer\r\n' +
        'Connection: close\r\n' +
        '\r\n' +
        '0\r\n' +
        'X-Client-Trailer: bar\r\n' +
        '\r\n',
      );
    }));

    client.on('data', () => {});

    client.on('end', common.mustCall(() => {
      server.close();
    }));
  }));
}

// Test 3: req.headers with __proto__ header (should not pollute prototype)
{
  const server = http.createServer(common.mustCall((req, res) => {
    assert.strictEqual(Object.getPrototypeOf(req.headers), null);
    // The __proto__ header should be stored as a regular property
    assert.strictEqual(
      Object.getOwnPropertyDescriptor(req.headers, '__proto__').value, 'test',
    );
    res.end();
  }));

  server.listen(0, common.mustCall(() => {
    const port = server.address().port;

    const client = net.connect(port, common.mustCall(() => {
      client.write(
        'GET / HTTP/1.1\r\n' +
        'Host: localhost\r\n' +
        '__proto__: test\r\n' +
        'Connection: close\r\n' +
        '\r\n',
      );
    }));

    client.on('end', common.mustCall(() => {
      server.close();
    }));

    client.resume();
  }));
}
