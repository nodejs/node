// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');
const MAX = +(process.argv[2] || 16 * 1024); // Command line option, or 16KB.

const { getOptionValue } = require('internal/options');

console.log('pid is', process.pid);
console.log('max header size is', getOptionValue('--max-http-header-size'));

// Verify that we cannot receive more than 16KB of headers.

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
  ['abort', 'error', 'end'].forEach((e) => {
    client.on(e, once(() => setImmediate(callback)));
  });
}

function fillHeaders(headers, currentSize, valid = false) {
  // `llhttp` counts actual header name/value sizes, excluding the whitespace
  // and stripped chars.
  // OK, Content-Length, 0, X-CRASH, aaa...
  headers += 'a'.repeat(MAX - currentSize);

  // Generate valid headers
  if (valid) {
    headers = headers.slice(0, -1);
  }
  return headers + '\r\n\r\n';
}

function writeHeaders(socket, headers) {
  const array = [];
  const chunkSize = 100;
  let last = 0;

  for (let i = 0; i < headers.length / chunkSize; i++) {
    const current = (i + 1) * chunkSize;
    array.push(headers.slice(last, current));
    last = current;
  }

  // Safety check we are chunking correctly
  assert.strictEqual(array.join(''), headers);

  next();

  function next() {
    if (socket.destroyed) {
      console.log('socket was destroyed early, data left to write:',
                  array.join('').length);
      return;
    }

    const chunk = array.shift();

    if (chunk) {
      console.log('writing chunk of size', chunk.length);
      socket.write(chunk, next);
    } else {
      socket.end();
    }
  }
}

function test1() {
  console.log('test1');
  let headers =
    'HTTP/1.1 200 OK\r\n' +
    'Content-Length: 0\r\n' +
    'X-CRASH: ';

  // OK, Content-Length, 0, X-CRASH, aaa...
  const currentSize = 2 + 14 + 1 + 7;
  headers = fillHeaders(headers, currentSize);

  const server = net.createServer((sock) => {
    sock.once('data', () => {
      writeHeaders(sock, headers);
      sock.resume();
    });

    // The socket might error but that's ok
    sock.on('error', () => {});
  });

  server.listen(0, common.mustCall(() => {
    const port = server.address().port;
    const client = http.get({ port: port }, common.mustNotCall());

    client.on('error', common.mustCall((err) => {
      assert.strictEqual(err.code, 'HPE_HEADER_OVERFLOW');
      server.close(test2);
    }));
  }));
}

const test2 = common.mustCall(() => {
  console.log('test2');
  let headers =
    'GET / HTTP/1.1\r\n' +
    'Host: localhost\r\n' +
    'Agent: nod2\r\n' +
    'X-CRASH: ';

  // /, Host, localhost, Agent, node, X-CRASH, a...
  const currentSize = 1 + 4 + 9 + 5 + 4 + 7;
  headers = fillHeaders(headers, currentSize);

  const server = http.createServer(common.mustNotCall());

  server.once('clientError', common.mustCall((err) => {
    assert.strictEqual(err.code, 'HPE_HEADER_OVERFLOW');
  }));

  server.listen(0, common.mustCall(() => {
    const client = net.connect(server.address().port);
    client.on('connect', () => {
      writeHeaders(client, headers);
      client.resume();
    });

    finished(client, common.mustCall(() => {
      server.close(test3);
    }));
  }));
});

const test3 = common.mustCall(() => {
  console.log('test3');
  let headers =
    'GET / HTTP/1.1\r\n' +
    'Host: localhost\r\n' +
    'Agent: nod3\r\n' +
    'X-CRASH: ';

  // /, Host, localhost, Agent, node, X-CRASH, a...
  const currentSize = 1 + 4 + 9 + 5 + 4 + 7;
  headers = fillHeaders(headers, currentSize, true);

  console.log('writing', headers.length);

  const server = http.createServer(common.mustCall((req, res) => {
    res.end('hello from test3 server');
    server.close();
  }));

  server.on('clientError', (err) => {
    console.log(err.code);
    if (err.code === 'HPE_HEADER_OVERFLOW') {
      console.log(err.rawPacket.toString('hex'));
    }
  });
  server.on('clientError', common.mustNotCall());

  server.listen(0, common.mustCall(() => {
    const client = net.connect(server.address().port);
    client.on('connect', () => {
      writeHeaders(client, headers);
      client.resume();
    });

    client.pipe(process.stdout);
  }));
});

test1();
