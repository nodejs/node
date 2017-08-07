// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

const server = http2.createServer();

server.on('stream', common.mustCall((stream) => {
  // Date header is defaulted
  stream.respond();
  stream.end();
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);
  const req = client.request();
  req.on('response', common.mustCall((headers) => {
    // The date header must be set to a non-invalid value
    assert.notStrictEqual((new Date()).toString(), 'Invalid Date');
  }));
  req.resume();
  req.on('end', common.mustCall(() => {
    server.close();
    client.destroy();
  }));
}));
