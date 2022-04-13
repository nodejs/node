'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');
const NGHTTP2_INTERNAL_ERROR = h2.constants.NGHTTP2_INTERNAL_ERROR;
const Countdown = require('../common/countdown');

const server = h2.createServer();

// Do not mustCall the server side callbacks, they may or may not be called
// depending on the OS. The determination is based largely on operating
// system specific timings
server.on('stream', (stream) => {
  // Do not wrap in a must call or use common.expectsError (which now uses
  // must call). The error may or may not be reported depending on operating
  // system specific timings.
  stream.on('error', (err) => {
    assert.strictEqual(err.code, 'ERR_HTTP2_STREAM_ERROR');
    assert.strictEqual(err.message,
                       'Stream closed with error code NGHTTP2_INTERNAL_ERROR');
  });
  stream.respond();
  stream.end();
});

server.listen(0, common.mustCall(() => {
  const client = h2.connect(`http://localhost:${server.address().port}`);
  const countdown = new Countdown(2, () => {
    server.close();
    client.close();
  });
  client.on('connect', () => countdown.dec());

  const req = client.request();
  req.destroy(new Error('test'));

  req.on('error', common.expectsError({
    name: 'Error',
    message: 'test'
  }));

  req.on('close', common.mustCall(() => {
    assert.strictEqual(req.rstCode, NGHTTP2_INTERNAL_ERROR);
    assert.strictEqual(req.rstCode, NGHTTP2_INTERNAL_ERROR);
    countdown.dec();
  }));

  req.on('response', common.mustNotCall());
  req.resume();
  req.on('end', common.mustNotCall());
}));
