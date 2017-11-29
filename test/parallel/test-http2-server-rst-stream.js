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
  ['rstStream', NGHTTP2_NO_ERROR, false],
  ['rstWithNoError', NGHTTP2_NO_ERROR, false],
  ['rstWithProtocolError', NGHTTP2_PROTOCOL_ERROR, true],
  ['rstWithCancel', NGHTTP2_CANCEL, false],
  ['rstWithRefuse', NGHTTP2_REFUSED_STREAM, true],
  ['rstWithInternalError', NGHTTP2_INTERNAL_ERROR, true]
];

const server = http2.createServer();
server.on('stream', (stream, headers) => {
  const method = headers['rstmethod'];
  stream[method]();
});

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);

  const countdown = new Countdown(tests.length, common.mustCall(() => {
    client.destroy();
    server.close();
  }));

  tests.forEach((test) => {
    const req = client.request({
      ':method': 'POST',
      rstmethod: test[0]
    });
    req.on('close', common.mustCall((code) => {
      assert.strictEqual(code, test[1]);
      countdown.dec();
    }));
    req.on('aborted', common.mustCall());
    if (test[2])
      req.on('error', common.mustCall());
    else
      req.on('error', common.mustNotCall());
  });
}));
