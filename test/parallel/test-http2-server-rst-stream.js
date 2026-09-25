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

// Each entry: [rstcode, errorCode]. A peer-initiated RST that arrives
// before END_STREAM surfaces 'aborted' + 'error' — clean codes
// (NO_ERROR/CANCEL) yield ERR_HTTP2_STREAM_ABORTED, other codes yield
// ERR_HTTP2_STREAM_ERROR.
const tests = [
  [NGHTTP2_NO_ERROR, 'ERR_HTTP2_STREAM_ABORTED'],
  [NGHTTP2_PROTOCOL_ERROR, 'ERR_HTTP2_STREAM_ERROR'],
  [NGHTTP2_CANCEL, 'ERR_HTTP2_STREAM_ABORTED'],
  [NGHTTP2_REFUSED_STREAM, 'ERR_HTTP2_STREAM_ERROR'],
  [NGHTTP2_INTERNAL_ERROR, 'ERR_HTTP2_STREAM_ERROR'],
];

const server = http2.createServer();
server.on('stream', common.mustCall((stream, headers) => {
  const code = headers.rstcode | 0;
  // Locally-initiated close synthesizes 'error' iff the code isn't clean
  // (NO_ERROR/CANCEL). Assert exactly that.
  if (code === NGHTTP2_NO_ERROR || code === NGHTTP2_CANCEL) {
    stream.on('error', common.mustNotCall(
      "'error' must not fire on locally-initiated close with a clean code"));
  } else {
    stream.on('error', common.mustCall((err) => {
      assert.strictEqual(err.code, 'ERR_HTTP2_STREAM_ERROR');
    }));
  }
  stream.close(code);
}, tests.length));

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
    req.on('error', common.mustCall((err) => {
      assert.strictEqual(err.code, test[1]);
    }));
  });
}));
