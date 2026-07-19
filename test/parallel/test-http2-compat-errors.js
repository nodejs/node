'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

// Errors on the underlying stream surface on Http2ServerRequest

let expected = null;

const server = h2.createServer(common.mustCall(function(req, res) {
  res.stream.on('error', common.mustCall((err) => {
    assert.strictEqual(err, expected);
  }));
  req.on('error', common.mustCall((err) => {
    assert.strictEqual(err, expected);
  }));
  res.on('error', common.mustNotCall());
  req.on('aborted', common.mustCall());
  res.on('aborted', common.mustNotCall());

  res.write('hello');

  expected = new Error('kaboom');
  res.stream.destroy(expected);
  server.close();
}));

server.listen(0, common.mustCall(function() {
  const url = `http://localhost:${server.address().port}`;
  const client = h2.connect(url, common.mustCall(() => {
    const request = client.request();
    request.on('data', common.mustCall((chunk) => {
      client.destroy();
    }));
  }));
}));
