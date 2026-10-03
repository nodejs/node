'use strict';

// Test that ClientRequest#end computes and sends a Content-Length header,
// regardless of the request method.
// Refs: https://github.com/nodejs/node/issues/27880

const common = require('../common');
const assert = require('assert');
const http = require('http');

const upload = 'PUT / HTTP/1.1\r\n\r\n';

const methods = ['GET', 'HEAD', 'DELETE', 'POST', 'PATCH', 'PUT', 'OPTIONS'];

const server = http.createServer(common.mustCall(function(req, res) {
  const chunks = [];
  req.on('data', (chunk) => chunks.push(chunk));
  req.on('end', common.mustCall(() => {
    assert.deepStrictEqual(Buffer.concat(chunks), Buffer.from(upload));
    assert.strictEqual(req.headers['content-length'],
                       String(upload.length));
    assert.strictEqual(req.headers['transfer-encoding'], undefined);

    // A HEAD response carries no body, so report the outcome in a header
    // instead of writing it to the response stream.
    res.setHeader('x-received-method', req.method);
    res.end();
  }));
}, methods.length));

server.listen(0, common.mustCall(function tryNextRequest() {
  const method = methods.pop();
  if (method === undefined) return;
  const port = server.address().port;
  const req = http.request({ method, port }, common.mustCall((res) => {
    assert.strictEqual(res.headers['x-received-method'], method);
    res.resume();
    res.on('end', common.mustCall(tryNextRequest));
  }));

  req.end(upload);
})).unref();
