'use strict';
const common = require('../common');
const assert = require('assert');

const http = require('http');

// A DELETE request with a payload must be framed like any other request, so
// that the payload is received as a body instead of being parsed as the start
// of a following request.
// Refs: https://github.com/nodejs/node/issues/27880

const data = 'PUT / HTTP/1.1\r\n\r\n';

const server = http.createServer(common.mustCall(function(req, res) {
  const chunks = [];
  req.on('data', (chunk) => chunks.push(chunk));
  req.on('end', common.mustCall(() => {
    assert.deepStrictEqual(Buffer.concat(chunks), Buffer.from(data));
    assert.strictEqual(req.headers['transfer-encoding'], 'chunked');

    res.setHeader('Content-Type', 'text/plain');
    res.end();
  }));
})).unref();

server.listen(0, common.mustCall(() => {
  const port = server.address().port;
  const req = http.request({ method: 'DELETE', port }, common.mustCall((res) => {
    res.resume();
  }));

  req.write(data);
  req.end();
}));
