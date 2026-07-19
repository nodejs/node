'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const http2 = require('http2');
const fs = require('fs');

const fname = fixtures.path('elipses.txt');

const server = http2.createServer();

server.on('stream', common.mustCall((stream) => {
  const originalClose = fs.close;
  let fdClosed = false;

  fs.close = common.mustCall(function(fd, cb) {
    fdClosed = true;
    return originalClose.apply(this, arguments);
  });

  const headers = {
    ':method': 'GET',
    'content-type': 'text/plain'
  };

  stream.respondWithFile(fname, headers);

  stream.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_HTTP2_INVALID_PSEUDOHEADER');
  }));

  stream.on('close', common.mustCall(() => {
    fs.close = originalClose;
    assert.strictEqual(fdClosed, true);
  }));
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);
  const req = client.request();

  req.on('close', common.mustCall(() => {
    client.close();
    server.close();
  }));

  req.on('error', common.mustCall());
  req.end();
}));
