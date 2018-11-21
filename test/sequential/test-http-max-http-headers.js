'use strict';

const assert = require('assert');
const common = require('../common');
const http = require('http');
const net = require('net');
const MAX = 8 * 1024; // 8KB

// Verify that we cannot receive more than 8KB of headers.

function once(cb) {
  let called = false;
  return () => {
    if (!called) {
      called = true;
      cb();
    }
  };
}

function finished(client, callback) {
  'abort error end'.split(' ').forEach((e) => {
    client.on(e, once(() => setImmediate(callback)));
  });
}

function fillHeaders(headers, currentSize, valid = false) {
  // llhttp counts actual header name/value sizes, excluding the whitespace and
  // stripped chars.
  if (process.versions.hasOwnProperty('llhttp')) {
    // OK, Content-Length, 0, X-CRASH, aaa...
    headers += 'a'.repeat(MAX - currentSize);
  } else {
    headers += 'a'.repeat(MAX - headers.length - 3);
  }

  // Generate valid headers
  if (valid) {
    // TODO(mcollina): understand why -9 is needed instead of -1
    headers = headers.slice(0, -9);
  }
  return headers + '\r\n\r\n';
}

const timeout = common.platformTimeout(10);

function writeHeaders(socket, headers) {
  const array = [];

  // this is off from 1024 so that \r\n does not get split
  const chunkSize = 1000;
  let last = 0;

  for (let i = 0; i < headers.length / chunkSize; i++) {
    const current = (i + 1) * chunkSize;
    array.push(headers.slice(last, current));
    last = current;
  }

  // safety check we are chunking correctly
  assert.strictEqual(array.join(''), headers);

  next();

  function next() {
    if (socket.write(array.shift())) {
      if (array.length === 0) {
        socket.end();
      } else {
        setTimeout(next, timeout);
      }
    } else {
      socket.once('drain', next);
    }
  }
}

function test1() {
  let headers =
    'HTTP/1.1 200 OK\r\n' +
    'Content-Length: 0\r\n' +
    'X-CRASH: ';

  // OK, Content-Length, 0, X-CRASH, aaa...
  const currentSize = 2 + 14 + 1 + 7;
  headers = fillHeaders(headers, currentSize);

  const server = net.createServer((sock) => {
    sock.once('data', (chunk) => {
      writeHeaders(sock, headers);
      sock.resume();
    });
  });

  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    const client = http.get({ port: port }, common.mustNotCall(() => {}));

    client.on('error', common.mustCall((err) => {
      assert.strictEqual(err.code, 'HPE_HEADER_OVERFLOW');
      server.close();
      setImmediate(test2);
    }));
  }));
}

const test2 = common.mustCall(() => {
  let headers =
    'GET / HTTP/1.1\r\n' +
    'Host: localhost\r\n' +
    'Agent: node\r\n' +
    'X-CRASH: ';

  // /, Host, localhost, Agent, node, X-CRASH, a...
  const currentSize = 1 + 4 + 9 + 5 + 4 + 7;
  headers = fillHeaders(headers, currentSize);

  const server = http.createServer(common.mustNotCall());

  server.on('clientError', common.mustCall((err) => {
    assert.strictEqual(err.code, 'HPE_HEADER_OVERFLOW');
  }));

  server.listen(0, common.mustCall(() => {
    const client = net.connect(server.address().port);
    client.on('connect', () => {
      writeHeaders(client, headers);
      client.resume();
    });

    finished(client, common.mustCall((err) => {
      server.close();
      setImmediate(test3);
    }));
  }));
});

const test3 = common.mustCall(() => {
  let headers =
    'GET / HTTP/1.1\r\n' +
    'Host: localhost\r\n' +
    'Agent: node\r\n' +
    'X-CRASH: ';

  // /, Host, localhost, Agent, node, X-CRASH, a...
  const currentSize = 1 + 4 + 9 + 5 + 4 + 7;
  headers = fillHeaders(headers, currentSize, true);

  const server = http.createServer(common.mustCall((req, res) => {
    res.end('hello world');
    setImmediate(server.close.bind(server));
  }));

  server.listen(0, common.mustCall(() => {
    const client = net.connect(server.address().port);
    client.on('connect', () => {
      writeHeaders(client, headers);
      client.resume();
    });
  }));
});

test1();
