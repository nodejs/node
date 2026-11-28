'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

// The server replies as soon as it has the request headers, while the client
// is still writing a large body with backpressure. Once the response is
// complete the client must keep forwarding the socket's 'drain' to the
// request, otherwise a writer that honors backpressure (for example a stream
// piped into the request) can never finish writing.

const BODY = 8 * 1024 * 1024;
const chunk = Buffer.alloc(64 * 1024);

const server = http.createServer(common.mustCall((req, res) => {
  res.writeHead(201, { 'Content-Length': '0' });
  res.end();
  req.resume();
}));

server.listen(0, common.mustCall(() => {
  const req = http.request({
    port: server.address().port,
    method: 'PUT',
    headers: { 'Content-Length': BODY },
  }, common.mustCall((res) => {
    assert.strictEqual(res.statusCode, 201);
    res.resume();
  }));

  req.on('finish', common.mustCall(() => {
    assert.strictEqual(req.writableNeedDrain, false);
    assert.strictEqual(req.writableFinished, true);
    server.close();
  }));

  let left = BODY;
  function write() {
    while (left > 0) {
      const n = Math.min(left, chunk.length);
      left -= n;
      if (!req.write(chunk.subarray(0, n))) {
        req.once('drain', write);
        return;
      }
    }
    req.end();
  }
  write();
}));
