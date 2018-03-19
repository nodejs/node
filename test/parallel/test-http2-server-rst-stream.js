'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');
const Countdown = require('../common/countdown');

const {
  NGHTTP2_CANCEL,
  NGHTTP2_NO_ERROR,
  NGHTTP2_PROTOCOL_ERROR,
  NGHTTP2_REFUSED_STREAM,
  NGHTTP2_INTERNAL_ERROR
} = http2.constants;

const tests = [
  [NGHTTP2_NO_ERROR, false],
  [NGHTTP2_NO_ERROR, false],
  [NGHTTP2_PROTOCOL_ERROR, true, 'NGHTTP2_PROTOCOL_ERROR'],
  [NGHTTP2_CANCEL, false],
  [NGHTTP2_REFUSED_STREAM, true, 'NGHTTP2_REFUSED_STREAM'],
  [NGHTTP2_INTERNAL_ERROR, true, 'NGHTTP2_INTERNAL_ERROR']
];

const server = http2.createServer();
server.on('stream', (stream, headers) => {
  const test = tests.find((t) => t[0] === Number(headers.rstcode));
  if (test[1]) {
    stream.on('error', common.expectsError({
      type: Error,
      code: 'ERR_HTTP2_STREAM_ERROR',
      message: `Stream closed with error code ${test[2]}`
    }));
  }
  stream.close(headers.rstcode | 0);
});

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);

  const countdown = new Countdown(tests.length, () => {
    client.close();
    server.close();
  });

  tests.forEach((test) => {
    const req = client.request({
      ':method': 'POST',
      'rstcode': test[0]
    });
    req.on('close', common.mustCall(() => {
      assert.strictEqual(req.rstCode, test[0]);
      countdown.dec();
    }));
    req.on('aborted', common.mustCall());
    if (test[1])
      req.on('error', common.mustCall());
    else
      req.on('error', common.mustNotCall());
  });
}));
