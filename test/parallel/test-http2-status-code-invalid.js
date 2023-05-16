'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

const server = http2.createServer();

function expectsError(code) {
  return common.expectsError({
    code: 'ERR_HTTP2_STATUS_INVALID',
    name: 'RangeError',
    message: `Invalid status code: ${code}`
  });
}

server.on('stream', common.mustCall((stream) => {

  // Anything lower than 100 and greater than 599 is rejected
  [ 99, 700, 1000 ].forEach((i) => {
    assert.throws(() => stream.respond({ ':status': i }), expectsError(i));
  });

  stream.respond();
  stream.end();
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);
  const req = client.request();
  req.on('response', common.mustCall((headers) => {
    assert.strictEqual(headers[':status'], 200);
  }));
  req.resume();
  req.on('end', common.mustCall(() => {
    server.close();
    client.close();
  }));
}));
