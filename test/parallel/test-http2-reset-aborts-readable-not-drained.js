'use strict';

// When the peer sends RST_STREAM without first END_STREAMing, the
// consumer will never see 'end' - surface that as 'aborted' +
// 'error' + 'close' (no 'end', no 'finish').

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');
const { NGHTTP2_NO_ERROR } = http2.constants;

const server = http2.createServer();

server.on('stream', common.mustCall((stream) => {
  let abortedFiredAt = -1;
  let errorFiredAt = -1;
  let closeFiredAt = -1;
  let tick = 0;

  stream.on('aborted', common.mustCall(() => {
    abortedFiredAt = ++tick;
    // 'aborted' fires while the stream is still alive but flagged.
    assert.strictEqual(stream.aborted, true);
  }));

  stream.on('error', common.mustCall((err) => {
    errorFiredAt = ++tick;
    assert.ok(err && typeof err.code === 'string' &&
              err.code.startsWith('ERR_HTTP2_'),
              `expected an ERR_HTTP2_* error, got ${err?.code}`);
  }));

  stream.on('end', common.mustNotCall(
    "'end' must not fire - readable was not drained at reset"));
  stream.on('finish', common.mustNotCall(
    "'finish' must not fire - stream was reset, not ended"));

  stream.on('close', common.mustCall(() => {
    closeFiredAt = ++tick;
    assert.strictEqual(stream.aborted, true);
    assert.strictEqual(stream.destroyed, true);
    assert.ok(abortedFiredAt !== -1, "'aborted' must fire before 'close'");
    assert.ok(errorFiredAt !== -1, "'error' must fire before 'close'");
    assert.ok(abortedFiredAt < closeFiredAt &&
              errorFiredAt < closeFiredAt,
              `'close' must come last (aborted=${abortedFiredAt} ` +
              `error=${errorFiredAt} close=${closeFiredAt})`);
    server.close();
  }));
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);

  const cs = client.request({ ':method': 'POST' });
  cs.on('close', common.mustCall(() => client.close()));

  cs.write('payload that will never be consumed');
  // Send RST_STREAM. Note we deliberately do NOT call cs.end() - the
  // server will see no END_STREAM, only the reset.
  cs.close(NGHTTP2_NO_ERROR);
}));
