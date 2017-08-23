// Flags: --expose-http2 --expose-internals
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');
const { Http2Stream } = require('internal/http2/core');

// Errors should not be reported both in Http2ServerRequest
// and Http2ServerResponse

let expected = null;

const server = h2.createServer(common.mustCall(function(req, res) {
  req.on('error', common.mustNotCall());
  res.on('error', common.mustNotCall());
  req.on('aborted', common.mustCall());
  res.on('aborted', common.mustNotCall());

  res.write('hello');

  expected = new Error('kaboom');
  res.stream.destroy(expected);
  server.close();
}));

server.on('streamError', common.mustCall(function(err, stream) {
  assert.strictEqual(err, expected);
  assert.strictEqual(stream instanceof Http2Stream, true);
}));

server.listen(0, common.mustCall(function() {
  const port = server.address().port;

  const url = `http://localhost:${port}`;
  const client = h2.connect(url, common.mustCall(function() {
    const headers = {
      ':path': '/foobar',
      ':method': 'GET',
      ':scheme': 'http',
      ':authority': `localhost:${port}`,
    };
    const request = client.request(headers);
    request.on('data', common.mustCall(function(chunk) {
      // cause an error on the server side
      client.destroy();
    }));
    request.end();
  }));
}));
