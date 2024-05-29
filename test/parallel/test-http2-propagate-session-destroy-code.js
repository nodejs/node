'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const http2 = require('http2');
const server = http2.createServer();
const sessionErrorMessage = 'Session closed with error code NGHTTP2_REFUSED_STREAM';
const streamErrorMessage = 'Stream closed with error code NGHTTP2_REFUSED_STREAM';
const destroyCode = http2.constants.NGHTTP2_REFUSED_STREAM;

server.on('error', common.mustNotCall());

server.on('session', (session) => {
  session.on('close', common.mustCall());
  session.on('error', common.mustCall((err) => {
    assert.strictEqual(err.message, sessionErrorMessage);
    assert.strictEqual(session.closed, false);
    assert.strictEqual(session.destroyed, true);
  }));

  session.on('stream', common.mustCall((stream) => {
    stream.on('error', common.mustCall((err) => {
      assert.strict(err.message, streamErrorMessage);
      assert.strictEqual(session.closed, false);
      assert.strictEqual(session.destroyed, true);
      assert.strictEqual(stream.rstCode, destroyCode);
    }));

    session.destroy(destroyCode);
  }));
});

server.listen(0, common.mustCall(() => {
  const session = http2.connect(`http://localhost:${server.address().port}`);

  session.on('error', common.mustCall((err) => {
    assert.strictEqual(err.message, sessionErrorMessage);
    assert.strictEqual(session.closed, false);
    assert.strictEqual(session.destroyed, true);
  }));

  const stream = session.request({ [http2.constants.HTTP2_HEADER_PATH]: '/' });

  stream.on('error', common.mustCall((err) => {
    assert.strictEqual(err.message, streamErrorMessage);
    assert.strictEqual(stream.rstCode, destroyCode);
  }));

  stream.on('close', common.mustCall(() => {
    server.close();
  }));
}));
