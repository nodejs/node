'use strict';
const common = require('../common');

// This test ensures that a Trailer header is set only when a chunked transfer
// encoding is used.

const assert = require('assert');
const http = require('http');

const server = http.createServer(common.mustCall(function(req, res) {
  res.setHeader('Trailer', 'baz');
  const trailerInvalidErr = {
    code: 'ERR_HTTP_TRAILER_INVALID',
    message: 'Trailers are invalid with this transfer encoding',
    name: 'Error'
  };
  assert.throws(() => res.writeHead(200, { 'Content-Length': '2' }),
                trailerInvalidErr);
  res.removeHeader('Trailer');
  res.end('ok');
}));
server.listen(0, common.mustCall(() => {
  http.get({ port: server.address().port }, common.mustCall((res) => {
    assert.strictEqual(res.statusCode, 200);
    let buf = '';
    res.on('data', (chunk) => {
      buf += chunk;
    }).on('end', common.mustCall(() => {
      assert.strictEqual(buf, 'ok');
    }));
    server.close();
  }));
}));
