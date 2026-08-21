'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');
let client;

const server = h2.createServer();
server.on('stream', (stream) => {
  stream.on('close', common.mustCall(() => {
    client.close();
    server.close();
  }));
  stream.on('error', common.expectsError({
    code: 'ERR_HTTP2_STREAM_ERROR',
    name: 'Error',
    message: 'Stream closed with error code NGHTTP2_PROTOCOL_ERROR'
  }));
});

server.listen(0, common.mustCall(() => {
  client = h2.connect(`http://localhost:${server.address().port}`);
  const req = client.request();
  const closeCode = 1;

  assert.throws(
    () => req.close(2 ** 32),
    {
      name: 'RangeError',
      code: 'ERR_OUT_OF_RANGE',
      message: 'The value of "code" is out of range. It must be ' +
               '>= 0 && <= 4294967295. Received 4294967296'
    }
  );
  assert.strictEqual(req.closed, false);

  [true, 1, {}, [], null, 'test'].forEach((notFunction) => {
    assert.throws(
      () => req.close(closeCode, notFunction),
      {
        name: 'TypeError',
        code: 'ERR_INVALID_ARG_TYPE',
      }
    );
    assert.strictEqual(req.closed, false);
  });

  req.close(closeCode, common.mustCall());
  assert.strictEqual(req.closed, true);

  // Make sure that destroy is called.
  req._destroy = common.mustCall(req._destroy.bind(req));

  // Second call doesn't do anything.
  req.close(closeCode + 1);

  req.on('close', common.mustCall(() => {
    assert.strictEqual(req.destroyed, true);
    assert.strictEqual(req.rstCode, closeCode);
  }));

  req.on('error', common.expectsError({
    code: 'ERR_HTTP2_STREAM_ERROR',
    name: 'Error',
    message: 'Stream closed with error code NGHTTP2_PROTOCOL_ERROR'
  }));

  // The `response` event should not fire as the server should receive the
  // RST_STREAM frame before it ever has a chance to reply.
  req.on('response', common.mustNotCall());

  // The `end` event should still fire as we close the readable stream by
  // pushing a `null` chunk.
  req.on('end', common.mustCall());

  req.resume();
  req.end();
}));
