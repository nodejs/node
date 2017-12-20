'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const fs = require('fs');
const http2 = require('http2');

const {
  NGHTTP2_INTERNAL_ERROR
} = http2.constants;

const errorCheck = common.expectsError({
  code: 'ERR_HTTP2_STREAM_ERROR',
  type: Error,
  message: `Stream closed with error code ${NGHTTP2_INTERNAL_ERROR}`
}, 2);

const server = http2.createServer();
server.on('stream', (stream) => {
  let fd = 2;

  // Get first known bad file descriptor.
  try {
    while (fs.fstatSync(++fd));
  } catch (e) {
    // do nothing; we now have an invalid fd
  }

  stream.respondWithFD(fd);
  stream.on('error', common.mustCall(errorCheck));
});
server.listen(0, () => {

  const client = http2.connect(`http://localhost:${server.address().port}`);
  const req = client.request();

  req.on('response', common.mustCall());
  req.on('error', common.mustCall(errorCheck));
  req.on('data', common.mustNotCall());
  req.on('end', common.mustCall(() => {
    assert.strictEqual(req.rstCode, NGHTTP2_INTERNAL_ERROR);
    client.close();
    server.close();
  }));
  req.end();
});
