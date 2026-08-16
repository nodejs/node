'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

// Headers stay in C++ until rawHeaders / headers are read.
// Server Host / Expect checks must not force that materialization.

const server = http.createServer(common.mustCall((req, res) => {
  assert.strictEqual(req._hasHeader('host'), true);
  assert.strictEqual(req._hasHeader('x-test'), true);
  assert.strictEqual(req._hasHeader('x-missing'), false);
  assert.strictEqual(req._getHeader('x-test'), 'one, two');
  assert.strictEqual(req._hasBodyHeaders(), false);

  // First public access materializes JS strings.
  assert.strictEqual(req.headers.host, `localhost:${server.address().port}`);
  assert.strictEqual(req.headers['x-test'], 'one, two');
  assert.ok(Array.isArray(req.rawHeaders));
  assert.ok(req.rawHeaders.includes('X-Test'));
  res.end('ok');
}));

server.listen(0, common.mustCall(() => {
  const port = server.address().port;
  http.get({
    port,
    headers: {
      'X-Test': ['one', 'two'],
    },
  }, common.mustCall((res) => {
    res.resume();
    res.on('end', common.mustCall(() => {
      // Requests without Host are still rejected without building headers.
      const client = net.connect(port, common.mustCall(() => {
        client.write('GET / HTTP/1.1\r\n\r\n');
      }));
      const chunks = [];
      client.on('data', (c) => chunks.push(c));
      client.on('end', common.mustCall(() => {
        const raw = Buffer.concat(chunks).toString('latin1');
        assert.match(raw, /^HTTP\/1\.1 400 /);
        server.close();
      }));
    }));
  }));
}));
